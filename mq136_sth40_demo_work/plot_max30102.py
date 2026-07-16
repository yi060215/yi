"""
MAX30102 Real-Time PPG Plot
Usage: python plot_max30102.py COM5
"""
import sys
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from collections import deque
import serial
import threading

PORT = sys.argv[1] if len(sys.argv) > 1 else 'COM5'
BAUD = 115200

red_data = deque(maxlen=500)
ir_data  = deque(maxlen=500)
hr_data  = deque(maxlen=500)
spo2_data = deque(maxlen=500)

def read_serial():
    ser = serial.Serial(PORT, BAUD, timeout=1)
    started = False
    while True:
        try:
            line = ser.readline().decode('utf-8', errors='ignore').strip()
            if not line: continue
            if 'RED,IR,HR,SpO2' in line:
                started = True
                continue
            if not started: continue
            parts = line.split(',')
            if len(parts) >= 4:
                r = int(parts[0]); i = int(parts[1])
                h = int(parts[2]); s = int(parts[3])
                if r > 0:
                    red_data.append(r); ir_data.append(i)
                    hr_data.append(h); spo2_data.append(s)
        except: pass

threading.Thread(target=read_serial, daemon=True).start()

fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(10, 6))
fig.canvas.manager.set_window_title('MAX30102 PPG Monitor')

def animate(frame):
    ax1.clear(); ax2.clear()
    xs = list(range(len(red_data)))
    if xs:
        ax1.plot(xs, list(red_data), 'r-', lw=0.8, label='RED')
        ax1.plot(xs, list(ir_data),  'b-', lw=0.8, label='IR')
        ax1.legend(loc='upper right')
        ax1.set_ylabel('ADC Value')
        ax1.set_title('PPG Waveform')

        h = list(hr_data); s = list(spo2_data)
        valid_h = [v if v > 0 and v < 250 else None for v in h]
        valid_s = [v if v > 0 and v < 100 else None for v in s]
        ax2.plot(xs, valid_h, 'g-', lw=1.0, label='HR (bpm)')
        ax2.plot(xs, valid_s, 'm-', lw=1.0, label='SpO2 (%)')
        ax2.legend(loc='upper right')
        ax2.set_ylabel('Value')
        ax2.set_xlabel('Sample')

ani = animation.FuncAnimation(fig, animate, interval=200)
plt.tight_layout()
plt.show()
