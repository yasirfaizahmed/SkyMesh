# SkyMesh

SkyMesh is an Arduino-based LoRa messaging project using NodeMCU ESP8266 boards, RA-02 LoRa modules, and SSD1306 OLED displays.

This repository currently includes:

- `examples/transmitter/transmitter.ino` — simple sender using rotary encoder updates.
- `examples/reciever/reciever.ino` — simple receiver display sketch.
- `examples/Duplex/Duplex.ino` — full two-way UI with compose/send/receive/save features.

---

## Features

- LoRa communication at **433 MHz** (`LoRa.begin(433E6)`)
- OLED output (SSD1306, I2C, 128x64)
- Rotary encoder input
- Duplex mode (single-node send + receive)
- EEPROM-backed saved messages and node ID persistence (Duplex example)

---

## Parts (BOM)

## Minimum (2-node test setup)

- 2x NodeMCU ESP8266 boards (or equivalent ESP8266 dev boards)
- 2x LoRa RA-02 (SX1278, 433 MHz) modules
- 2x SSD1306 I2C OLED displays (128x64, address `0x3C`)
- Breadboard + jumper wires
- Stable 3.3V power source (important for LoRa reliability)

## Optional (for Duplex UI)

- 1x Rotary encoder module with push switch (CLK/DT/SW)

---

## Connections

> All pins below are based on current sketches in this repo.

## NodeMCU ESP8266 ↔ OLED (SSD1306 I2C)

| OLED Pin | NodeMCU Pin | ESP8266 GPIO |
|---|---|---|
| VCC | 3V3 | — |
| GND | GND | — |
| SDA | D2 | GPIO4 |
| SCL | D1 | GPIO5 |

## NodeMCU ESP8266 ↔ LoRa RA-02 (as used in code)

| LoRa Pin | NodeMCU Pin | ESP8266 GPIO | Notes |
|---|---|---|---|
| NSS / CS | D8 | GPIO15 | `SS` in sketches |
| RST | D0 | GPIO16 | Used in transmitter/receiver |
| DIO0 | D4 | GPIO2 | Used in receiver |
| VCC | 3V3 | — | 3.3V only |
| GND | GND | — | common ground |

### Duplex sketch note

`examples/Duplex/Duplex.ino` currently uses:

```cpp
LoRa.setPins(SS, -1, -1);
```

So only **SS** is explicitly configured there (no explicit RST/DIO0 pin assignment).

## NodeMCU ESP8266 ↔ Rotary Encoder (Duplex)

| Encoder Pin | NodeMCU Pin | ESP8266 GPIO |
|---|---|---|
| CLK | D3 | GPIO0 |
| DT  | D4 | GPIO2 |
| SW  | D0 | GPIO16 |
| VCC | 3V3 | — |
| GND | GND | — |

---

## Software setup (Arduino IDE)

1. Install **Arduino IDE**.
2. Add ESP8266 board support:
   - Open **File → Preferences**
   - Add to “Additional Boards Manager URLs”:
     - `http://arduino.esp8266.com/stable/package_esp8266com_index.json`
   - Open **Boards Manager**, install **esp8266 by ESP8266 Community**.
3. Select board (example): **NodeMCU 1.0 (ESP-12E Module)**.
4. Install required libraries from Library Manager:
   - `LoRa` (sandeep mistry)
   - `Adafruit GFX Library`
   - `Adafruit SSD1306`
5. Connect board via USB and select the correct COM/serial port.

---

## Getting started

## A) Simple link test (transmitter + receiver)

1. Flash `examples/reciever/reciever.ino` to board A.
2. Flash `examples/transmitter/transmitter.ino` to board B.
3. Open Serial Monitor (9600 baud) on both boards.
4. Rotate encoder on transmitter board.
5. Confirm packets appear on receiver serial + OLED.

## B) Duplex mode test (recommended)

1. Flash `examples/Duplex/Duplex.ino` to 2 nodes.
2. Set different **SelfID** values on each node (e.g., 1 and 2).
3. Set each node’s **Target** to the other node’s ID.
4. Compose a message and use **SEND** to start beacon transmission.
5. Confirm receiving node shows RX updates.

For detailed per-screen UI behavior, see:

- `examples/Duplex/README.md`

---

## Troubleshooting

- **LoRa Error on startup**
  - Check frequency/module match (this project uses 433 MHz).
  - Recheck CS/RST/DIO0 wiring and common GND.
  - Ensure supply can handle LoRa current spikes.

- **OLED blank / not detected**
  - Verify `0x3C` I2C address and SDA/SCL wiring.
  - Check OLED power (3.3V vs board compatibility).

- **No received packets**
  - Confirm both nodes use same frequency and library setup.
  - In Duplex mode, verify target/self node IDs are correct.

- **Random resets / unstable behavior**
  - Improve 3.3V power quality.
  - Keep wiring short and stable.

---

## Repository structure

```text
documentation/
examples/
  Duplex/
    Duplex.ino
    README.md
  reciever/
    reciever.ino
  transmitter/
    transmitter.ino
  rotary_encoder/
    rotary_encoder.ino
```

---

## Notes

- The folder/sketch name `reciever` is kept as-is to match current repository naming.
- Pin mappings are taken from current code and may be updated as hardware evolves.
