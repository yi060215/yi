from __future__ import annotations

import copy
import time

from app import app


def build_payload(phone: str, tendency: str) -> dict:
    return {
        "cmd": "upload_record",
        "device_id": "RA8P1-SMOKE",
        "phone": phone,
        "gender": "female",
        "age": 23,
        "record": {
            "wangzhen": {},
            "wenzhen_audio": {},
            "wenzhen_question": {
                "sleep": "poor",
                "fatigue": "often",
            },
            "qiezhen": {},
            "summary": {
                "main_tendency": tendency,
            },
        },
    }


def main() -> None:
    phone = f"199{int(time.time()) % 100000000:08d}"
    first_payload = build_payload(phone, "smoke-first")
    second_payload = copy.deepcopy(first_payload)
    second_payload["record"]["summary"]["main_tendency"] = "smoke-second"

    with app.test_client() as client:
        first = client.post("/api/upload-record", json=first_payload)
        second = client.post("/api/upload-record", json=second_payload)
        patient = client.get(f"/api/patient/{phone}")
        code_resp = client.post("/api/mobile/send-code", json={"phone": phone})
        code = code_resp.get_json()["dev_code"]
        login_resp = client.post("/api/mobile/login", json={"phone": phone, "code": code})
        session_token = login_resp.get_json()["session_token"]
        records_resp = client.get(
            "/api/mobile/records",
            headers={"X-Session-Token": session_token},
        )
        qrcode_resp = client.get(f"/api/qrcode/{second.get_json()['qrcode_token']}")
        mobile_code_page = client.post("/mobile/send-code", data={"phone": phone})
        mobile_code = mobile_code_page.get_data(as_text=True).split("<strong>")[1].split("</strong>")[0]
        mobile_login_page = client.post(
            "/mobile/login",
            data={"phone": phone, "code": mobile_code},
            follow_redirects=True,
        )

    assert first.status_code == 200, first.get_data(as_text=True)
    assert second.status_code == 200, second.get_data(as_text=True)
    assert patient.status_code == 200, patient.get_data(as_text=True)
    assert code_resp.status_code == 200, code_resp.get_data(as_text=True)
    assert login_resp.status_code == 200, login_resp.get_data(as_text=True)
    assert records_resp.status_code == 200, records_resp.get_data(as_text=True)
    assert qrcode_resp.status_code == 200, qrcode_resp.get_data(as_text=True)
    assert mobile_code_page.status_code == 200, mobile_code_page.get_data(as_text=True)
    assert mobile_login_page.status_code == 200, mobile_login_page.get_data(as_text=True)

    second_json = second.get_json()
    patient_json = patient.get_json()
    mobile_records_json = records_resp.get_json()
    record_ids = patient_json["patient"]["record_ids"]

    assert len(record_ids) == 2, record_ids
    assert len(mobile_records_json["records"]) == 2, mobile_records_json
    assert second_json["compare_summary"]["items"], second_json
    assert qrcode_resp.get_json()["report"]["record_id"] == second_json["record_id"]
    assert second_json["record_id"] in mobile_login_page.get_data(as_text=True)

    print("Smoke test passed")
    print(f"phone={phone}")
    print(f"record1={first.get_json()['record_id']}")
    print(f"record2={second_json['record_id']}")
    print(f"history_count={len(record_ids)}")
    print(f"mobile_history_count={len(mobile_records_json['records'])}")
    print("mobile_web=ok")
    print("compare=" + " | ".join(second_json["compare_summary"]["items"]))


if __name__ == "__main__":
    main()
