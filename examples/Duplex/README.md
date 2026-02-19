# SkyMesh Duplex Example

This example (`Duplex.ino`) turns one ESP8266 + LoRa + OLED + rotary encoder node into a **send + receive** text terminal.

- Build a message character-by-character with the rotary encoder.
- Send it as a repeating unicast beacon to a selected target node ID.
- Receive packets addressed to this node and show them in the RX area.
- Save up to 5 messages in EEPROM and load them later.

---

## What this sketch does

The Duplex UI is state-based:

- `UI_HOME` → compose message
- `UI_MENU` → feature selection
- `UI_BEACON` → periodic transmit mode
- `UI_SAVED` → browse/load saved messages
- `UI_TARGET` → edit target node ID
- `UI_SELF_ID` → edit this node’s ID

Node IDs are one-to-one (1..9). Packets are plain text in this format:

`T:<target>|F:<from>|M:<message>`

Only packets where `target == myNodeId` are accepted/displayed.

---

## Hardware / Pin mapping (as used in code)

### LoRa (RA-02)
- `SS` → GPIO15

### OLED (SSD1306 I2C)
- `SDA` → GPIO4
- `SCL` → GPIO5
- I2C address: `0x3C`

### Rotary Encoder
- `CLK` → GPIO0
- `DT`  → GPIO2
- `SW`  → GPIO16

---

## Controls

- **Rotate encoder**: change current selection/value.
- **Single click**: confirm/select/action.
- **Double click**: open/close menu or go back (depending on screen).

Double-click detection window: ~300 ms.

---

## UI flow by feature

## 1) Compose message (Home)

**Goal:** Build payload text.

Flow:
1. Device boots into **Home**.
2. Rotate encoder to choose character (`space`, `a-z`, `0-9`).
3. Single click to append selected character to payload.
4. Double click to open Menu.

Notes:
- Payload is shown in a 2-line window (tail visible when long).
- RX preview line is shown at the bottom (scrolls if long).

---

## 2) Open Menu / Navigate Menu

Flow:
1. From **Home**, double click → **Menu**.
2. Rotate to move through options:
   - `Delete`, `Clear`, `Save`, `Saved`, `SelfID`, `Target`, `SEND`, `Back`
3. Single click executes current option.
4. Double click from Menu returns to Home.

---

## 3) Delete last character

Flow:
1. Home → double click (Menu)
2. Select `Delete`
3. Single click
4. Returns Home

Behavior:
- Removes only last character.
- If payload is empty, shows “Payload is empty”.

---

## 4) Clear payload

Flow:
1. Home → Menu
2. Select `Clear`
3. Single click
4. Returns Home

Behavior:
- Entire payload becomes empty.

---

## 5) Save payload to EEPROM

Flow:
1. Home → Menu
2. Select `Save`
3. Single click
4. Returns Home

Behavior:
- Stores current payload in circular EEPROM history.
- Capacity: **5 messages**, max **24 chars** each.
- New save becomes the newest entry.
- Empty payload is not saved.

---

## 6) Browse/load saved messages

Flow:
1. Home → Menu
2. Select `Saved`
3. In Saved screen, rotate to browse entries (newest first).
4. Single click loads selected saved message into payload and returns Home.
5. Double click exits to Menu.

Behavior:
- If no saved messages, screen shows “No saved messages”.

---

## 7) Set Self Node ID

Flow:
1. Home → Menu
2. Select `SelfID`
3. Rotate to choose ID (1..9)
4. Single click to confirm, save to EEPROM, return Menu
5. Double click cancels/backs to Menu without save

Behavior:
- Self ID persists across reboot.
- Target ID is sanitized to avoid invalid/self-conflicting values.

---

## 8) Set Target Node ID

Flow:
1. Home → Menu
2. Select `Target`
3. Rotate to choose target (1..9)
4. Single click to confirm and return Menu
5. Double click cancels/backs to Menu

Behavior:
- Target is adjusted if invalid or same as self.

---

## 9) Start/Stop SEND (Beacon mode)

Flow to start:
1. Home → Menu
2. Select `SEND`
3. Single click

If valid:
- Enters **Beacon** screen
- Sends unicast packet every ~2 seconds to target

If invalid:
- Empty payload → “Payload empty”
- Target == self → “Target cannot be self”
- Returns Home

Flow to stop:
- In Beacon screen, single click **or** double click → stop and return Home.

---

## 10) Receive messages (background feature)

Always active in loop:
1. Read LoRa packet.
2. Parse `T:|F:|M:` format.
3. Ignore packets not addressed to this node.
4. On valid packet for this node:
   - Show RX indicator (`RX*`) briefly
   - Update RX line/marquee
   - Show toast with sender + preview

This works while using all UI screens.

---

## Startup behavior

On boot:
1. OLED init + power-on animation.
2. EEPROM init/magic check.
3. Load saved messages metadata.
4. Load self node ID from EEPROM (default: `1`).
5. LoRa init at `433E6`.
6. Enter Home and show initial toast (`Me:<id> T:<id>`).

---

## Quick usage scenario

1. Set `SelfID` on each node (unique IDs).
2. Set `Target` to the node you want to message.
3. Compose message in Home.
4. `SEND` to start periodic transmission.
5. On receiver node, watch RX area for incoming message.
6. Save commonly used messages via `Save` and reuse from `Saved`.
