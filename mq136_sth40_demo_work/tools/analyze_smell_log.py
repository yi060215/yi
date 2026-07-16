#!/usr/bin/env python3
import argparse
import re
import statistics
from pathlib import Path


LINE_RE = re.compile(
    r"SMELL_LOG,"
    r"t_ms=(?P<t_ms>\d+),"
    r"raw=(?P<raw>\d+),"
    r"filtered=(?P<filtered>\d+),"
    r"mv=(?P<mv>\d+),"
    r"temp_c=(?P<temp_c>-?\d+(?:\.\d+)?),"
    r"rh=(?P<rh>-?\d+(?:\.\d+)?),"
    r"mq_valid=(?P<mq_valid>[01]),"
    r"sht_valid=(?P<sht_valid>[01]),"
    r"samples=(?P<samples>\d+),"
    r"sht_samples=(?P<sht_samples>\d+),"
    r"adc_ch=(?P<adc_ch>\d+),"
    r"adc_err=(?P<adc_err>-?\d+),"
    r"iic_err=(?P<iic_err>-?\d+)"
)

BANNER_RE = re.compile(r"SMELL SESSION #(?P<index>\d+)")

SESSION_RE = re.compile(
    r"SMELL_SESSION,"
    r"baseline=(?P<baseline>\d+),"
    r"breath_avg=(?P<breath_avg>\d+),"
    r"breath_peak=(?P<breath_peak>\d+),"
    r"final=(?P<final>\d+),"
    r"delta=(?P<delta>\d+),"
    r"abs_th=(?P<abs_th>\d+),"
    r"rel_th=(?P<rel_th>\d+),"
    r"base_temp_c=(?P<base_temp_c>-?\d+(?:\.\d+)?),"
    r"base_rh=(?P<base_rh>-?\d+(?:\.\d+)?),"
    r"breath_temp_c=(?P<breath_temp_c>-?\d+(?:\.\d+)?),"
    r"breath_rh=(?P<breath_rh>-?\d+(?:\.\d+)?),"
    r"bad=(?P<bad>[01])"
)


def parse_args():
    parser = argparse.ArgumentParser(
        description="Analyze SMELL_LOG output and estimate thresholds."
    )
    parser.add_argument("logfile", type=Path, help="Path to a text log containing SMELL_LOG lines")
    parser.add_argument("--baseline-start", type=int, default=0, help="Baseline window start in ms")
    parser.add_argument("--baseline-end", type=int, default=3000, help="Baseline window end in ms")
    parser.add_argument("--breath-start", type=int, default=3000, help="Breath window start in ms")
    parser.add_argument("--breath-end", type=int, default=8000, help="Breath window end in ms")
    parser.add_argument("--session-index", type=int, help="Only analyze one session index")
    return parser.parse_args()


def load_sessions(path: Path):
    sessions = []
    current = None
    next_index = 1

    for line in path.read_text(encoding="utf-8", errors="ignore").splitlines():
        banner = BANNER_RE.search(line)
        if banner:
            if current and (current["samples"] or current["summary"]):
                sessions.append(current)
            current = {
                "index": int(banner.group("index")),
                "samples": [],
                "summary": None,
            }
            next_index = current["index"] + 1
            continue

        match = LINE_RE.search(line)
        if match:
            if current is None:
                current = {
                    "index": next_index,
                    "samples": [],
                    "summary": None,
                }
                next_index += 1
            entry = match.groupdict()
            current["samples"].append(
                {
                    "t_ms": int(entry["t_ms"]),
                    "filtered": int(entry["filtered"]),
                    "raw": int(entry["raw"]),
                    "mv": int(entry["mv"]),
                    "mq_valid": int(entry["mq_valid"]),
                    "sht_valid": int(entry["sht_valid"]),
                    "temp_c": float(entry["temp_c"]),
                    "rh": float(entry["rh"]),
                }
            )
            continue

        session_match = SESSION_RE.search(line)
        if session_match:
            if current is None:
                current = {
                    "index": next_index,
                    "samples": [],
                    "summary": None,
                }
                next_index += 1
            current["summary"] = {
                key: (float(value) if "." in value else int(value))
                for key, value in session_match.groupdict().items()
            }

    if current and (current["samples"] or current["summary"]):
        sessions.append(current)

    return sessions


def window(samples, start_ms, end_ms):
    return [s for s in samples if start_ms <= s["t_ms"] < end_ms and s["mq_valid"] == 1]


def summarize(samples, baseline_start, baseline_end, breath_start, breath_end):
    baseline = window(samples, baseline_start, baseline_end)
    breath = window(samples, breath_start, breath_end)

    if not baseline:
        raise ValueError("No valid baseline samples found.")
    if not breath:
        raise ValueError("No valid breath samples found.")

    baseline_values = [s["filtered"] for s in baseline]
    breath_values = [s["filtered"] for s in breath]

    baseline_avg = round(statistics.mean(baseline_values))
    baseline_std = statistics.pstdev(baseline_values) if len(baseline_values) > 1 else 0.0
    breath_avg = round(statistics.mean(breath_values))
    breath_peak = max(breath_values)
    final_raw = round((breath_avg + breath_peak) / 2.0)
    delta_raw = max(0, final_raw - baseline_avg)

    relative_threshold = max(80, round(baseline_avg * 0.12))
    noise_guard = round(max(50.0, baseline_std * 4.0))
    suggested_threshold = max(relative_threshold, noise_guard)

    return {
        "sample_count": len(samples),
        "baseline_count": len(baseline),
        "breath_count": len(breath),
        "baseline_avg": baseline_avg,
        "baseline_std": baseline_std,
        "breath_avg": breath_avg,
        "breath_peak": breath_peak,
        "final_raw": final_raw,
        "delta_raw": delta_raw,
        "relative_threshold": relative_threshold,
        "noise_guard": noise_guard,
        "suggested_threshold": suggested_threshold,
        "baseline_temp_c": statistics.mean(s["temp_c"] for s in baseline),
        "baseline_rh": statistics.mean(s["rh"] for s in baseline),
        "breath_temp_c": statistics.mean(s["temp_c"] for s in breath),
        "breath_rh": statistics.mean(s["rh"] for s in breath),
    }


def print_report(session_index, report, firmware_summary=None):
    print(f"session_index={session_index}")
    print(f"sample_count={report['sample_count']}")
    print(f"baseline_count={report['baseline_count']}")
    print(f"breath_count={report['breath_count']}")
    print(f"baseline_avg={report['baseline_avg']}")
    print(f"baseline_std={report['baseline_std']:.2f}")
    print(f"breath_avg={report['breath_avg']}")
    print(f"breath_peak={report['breath_peak']}")
    print(f"final_raw={report['final_raw']}")
    print(f"delta_raw={report['delta_raw']}")
    print(f"relative_threshold_12pct={report['relative_threshold']}")
    print(f"noise_guard_4sigma={report['noise_guard']}")
    print(f"suggested_threshold={report['suggested_threshold']}")
    print(f"baseline_temp_c={report['baseline_temp_c']:.2f}")
    print(f"baseline_rh={report['baseline_rh']:.2f}")
    print(f"breath_temp_c={report['breath_temp_c']:.2f}")
    print(f"breath_rh={report['breath_rh']:.2f}")
    print(f"breath_rh_delta={report['breath_rh'] - report['baseline_rh']:.2f}")
    if firmware_summary is not None:
        print(f"fw_baseline={firmware_summary['baseline']}")
        print(f"fw_breath_avg={firmware_summary['breath_avg']}")
        print(f"fw_breath_peak={firmware_summary['breath_peak']}")
        print(f"fw_final={firmware_summary['final']}")
        print(f"fw_delta={firmware_summary['delta']}")
        print(f"fw_rel_th={firmware_summary['rel_th']}")
        print(f"fw_bad={firmware_summary['bad']}")


def main():
    args = parse_args()
    sessions = load_sessions(args.logfile)
    if not sessions:
        raise SystemExit("No SMELL_LOG lines found.")

    print(f"log_file={args.logfile}")
    for session in sessions:
        if args.session_index is not None and session["index"] != args.session_index:
            continue
        if not session["samples"]:
            continue

        report = summarize(
            session["samples"],
            args.baseline_start,
            args.baseline_end,
            args.breath_start,
            args.breath_end,
        )

        print("")
        print_report(session["index"], report, session["summary"])


if __name__ == "__main__":
    main()
