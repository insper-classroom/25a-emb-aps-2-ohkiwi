#!/usr/bin/env python3
import sys
import glob
import time
import serial
from collections import deque
from evdev import UInput, AbsInfo
from evdev.ecodes import EV_ABS, EV_KEY, ABS_X, ABS_Y, ABS_Z, BTN_TRIGGER, BTN_THUMB, BTN_TOP, BTN_PINKIE

# IDs recebidos pela UART (conforme RP2040)
ID_STEERING   = 15
ID_ACCEL      = 26
ID_BRAKE      = 27
ID_GEAR_UP    = 2
ID_GEAR_DOWN  = 3
ID_IGNITION   = 4
ID_LIGHTS     = 5

BUTTON_MAP = {
    ID_GEAR_UP:   BTN_TRIGGER,
    ID_GEAR_DOWN: BTN_THUMB,
    ID_IGNITION:  BTN_TOP,
    ID_LIGHTS:    BTN_PINKIE,
}

# configura o dispositivo virtual
capabilities = {
    EV_ABS: [
        (ABS_X, AbsInfo(-450, -450, 450, 0, 0, 0)),   # volante
        (ABS_Y, AbsInfo(   0,    0, 1023, 0, 0, 0)),  # acelerador
        (ABS_Z, AbsInfo(   0,    0, 1023, 0, 0, 0)),  # freio
    ],
    EV_KEY: list(BUTTON_MAP.values())
}
device = UInput(capabilities, name="Volante ETS2")

def find_serial_port():
    ports = glob.glob('/dev/ttyACM*') + glob.glob('/dev/ttyUSB*')
    return ports[0] if ports else None

def parse_pacote(buf):
    if len(buf) != 4 or buf[3] != 0xFF:
        return None
    return buf[0], int.from_bytes(buf[1:3], 'little')

def controle_serial(ser):
    buf = bytearray()
    # histórico de últimas N leituras do volante para média móvel
    hist = deque(maxlen=5)

    while True:
        b = ser.read(1)
        if not b:
            continue
        buf.append(b[0])
        if buf and buf[-1] == 0xFF and len(buf) >= 4:
            pkt = buf[-4:]
            buf.clear()
            res = parse_pacote(pkt)
            if not res:
                continue
            tipo, valor = res

            if tipo == ID_STEERING:
                # valor 0–100 → ângulo –450…+450
                ang = (valor / 100) * 900 - 450
                # adiciona ao histórico e calcula média móvel
                hist.append(ang)
                avg_ang = sum(hist) / len(hist)
                device.write(EV_ABS, ABS_X, int(avg_ang))

            elif tipo == ID_ACCEL:
                p = int((valor / 4095) * 1023)
                device.write(EV_ABS, ABS_Y, p)

            elif tipo == ID_BRAKE:
                p = int((valor / 4095) * 1023)
                device.write(EV_ABS, ABS_Z, p)

            elif tipo in BUTTON_MAP:
                code = BUTTON_MAP[tipo]
                device.write(EV_KEY, code, 1)
                device.syn()
                time.sleep(0.05)
                device.write(EV_KEY, code, 0)

            device.syn()

if __name__ == "__main__":
    port = find_serial_port()
    if not port:
        print("Nenhuma porta serial encontrada.")
        sys.exit(1)
    try:
        ser = serial.Serial(port, 115200, timeout=1)
        print(f"Conectado em {port}.")
        controle_serial(ser)
    except Exception as e:
        print(f"Erro na serial: {e}")
    finally:
        if 'ser' in locals():
            ser.close()
