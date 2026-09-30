# Beehive Monitoring Prototype Implementation Plan

> For AI agents: execute this plan in small verified steps. Build after every firmware milestone. Keep hardware assumptions explicit.

**Goal:** Build a verifiable ESP32-S3 + LoRa beehive environment monitoring prototype. This project (`F:/0GateWay/myproject`) is the gateway firmware: receive LoRa data frames, forward newline-delimited JSON to a PC, store and visualize records, then prepare data for GRU prediction.

**Architecture:** Future ESP32-S3-N16R8 sensor nodes sample temperature/humidity and send compact frames over LoRa. This gateway receives LoRa frames and forwards newline-delimited JSON over USB serial to a PC service. The PC service stores data in SQLite and exposes data for charting and later model training.

**Stack:** ESP-IDF 5.5.4, ESP32-S3-WROOM-1-N16R8, FreeRTOS, I2C temperature/humidity sensors, SPI LoRa module, Python, SQLite, Web chart UI.

---

## Confirmed Context

- Firmware project: `F:/0GateWay/myproject`
- Firmware role: LoRa gateway
- Main firmware file today: `F:/0GateWay/myproject/main/main.c`
- Component file today: `F:/0GateWay/myproject/main/CMakeLists.txt`
- Target board: ESP32-S3-WROOM-1-N16R8
- Flash/PSRAM: 16MB Quad SPI Flash, 8MB Octal SPI PSRAM
- Current firmware state: gateway board smoke test builds; LoRa receive and serial JSON logic are not implemented yet
- Current partition strategy: ESP-IDF default single app partition

## Hardware Decisions Before Coding External Drivers

- Select exact LoRa module. Recommended default for simple point-to-point prototype: SX1278/RA-02 class module if the purchased module is 3.3V logic and the frequency is legal for the test setup.
- Confirm gateway LoRa wiring before assigning final GPIOs. Avoid GPIO0, GPIO19, GPIO20, GPIO48, U0TXD, and U0RXD for normal external devices.
- Sensor module selection belongs to the future sensor-node firmware project, not this gateway firmware.

## Proposed Firmware File Structure

- Keep `F:/0GateWay/myproject/main/main.c`
  - ESP-IDF app entry point only: startup, top-level task wiring, and high-level application flow.
- Create `F:/0GateWay/myproject/components/app_common/`
  - Shared gateway config, received-frame structures, status flags, gateway role, LoRa parameters.
- Create `F:/0GateWay/myproject/components/board_support/`
  - Board, flash, heap, PSRAM, reset reason, and pin note logging.
- Create `F:/0GateWay/myproject/components/frame_codec/`
  - Encode/decode node data frames with version, node id, sequence, timestamp, payload, CRC.
- Create `F:/0GateWay/myproject/components/sensor_service/`
  - Reserved for future node-side sensor code; do not use in the gateway baseline.
- Create `F:/0GateWay/myproject/components/lora_service/`
  - LoRa SPI init, receive path, RSSI/SNR metadata, error/log hooks.
- Create `F:/0GateWay/myproject/components/serial_gateway/`
  - Print received/collected records as newline-delimited JSON for PC ingestion.
- Modify `F:/0GateWay/myproject/main/CMakeLists.txt`
  - Keep only the `main.c` registration and declare component dependencies as components become real.

## Proposed PC File Structure

- Create `F:/0GateWay/pc_gateway/serial_ingest.py`
  - Read USB serial JSON lines, validate fields, write SQLite.
- Create `F:/0GateWay/pc_gateway/storage.py`
  - SQLite schema, insert/query helpers.
- Create `F:/0GateWay/pc_gateway/app.py`
  - Small web/API service for latest values and history.
- Create `F:/0GateWay/pc_gateway/static/index.html`
  - Simple dashboard with latest values, history chart, and node status.
- Create `F:/0GateWay/pc_gateway/export_dataset.py`
  - Export ordered CSV for GRU training.

---

## Milestone 1: Gateway Board Bring-Up Baseline

**Files:**
- Modify `F:/0GateWay/myproject/main/main.c`
- Create `F:/0GateWay/myproject/components/board_support/CMakeLists.txt`
- Create `F:/0GateWay/myproject/components/board_support/include/board_info.h`
- Create `F:/0GateWay/myproject/components/board_support/src/board_info.c`
- Create `F:/0GateWay/myproject/components/app_common/CMakeLists.txt`
- Create `F:/0GateWay/myproject/components/app_common/include/app_config.h`
- Modify `F:/0GateWay/myproject/main/CMakeLists.txt`

- [ ] Move board info logging out of `main.c` into `board_info.c`.
- [ ] Keep PSRAM and flash reporting on boot.
- [ ] Remove Wi-Fi scan from the default path so the gateway baseline is not tied to Wi-Fi.
- [ ] Add `GATEWAY_ID` and gateway role config in `app_config.h`.
- [ ] Build with:

```powershell
. 'F:\AESP\Espressif\Initialize-Idf.ps1' -IdfId 'esp-idf-130bf6cdd4a9b4d9f11df0a2041d49f5'
idf.py build
```

- [ ] Flash and monitor once to verify the board reports `16 MB` flash and nonzero PSRAM.

## Milestone 2: Data Model and Frame Codec

**Files:**
- Create `F:/0GateWay/myproject/components/app_common/include/app_data.h`
- Create `F:/0GateWay/myproject/components/frame_codec/CMakeLists.txt`
- Create `F:/0GateWay/myproject/components/frame_codec/include/frame_codec.h`
- Create `F:/0GateWay/myproject/components/frame_codec/src/frame_codec.c`
- Modify `F:/0GateWay/myproject/main/CMakeLists.txt`

- [ ] Define a `sensor_sample_t` structure with node id, sequence number, uptime, inside temperature, inside humidity, outside temperature, outside humidity, and status bits.
- [ ] Define a compact LoRa frame layout with magic byte, version, node id, sequence, payload, and CRC16.
- [ ] Add encode/decode functions that return `esp_err_t`.
- [ ] Add a compile-time self-test path or a temporary boot-time codec check that logs pass/fail.
- [ ] Build and verify codec self-test logs before adding real LoRa.

## Milestone 3: Gateway Serial JSON Output

**Files:**
- Create `F:/0GateWay/myproject/components/serial_gateway/CMakeLists.txt`
- Create `F:/0GateWay/myproject/components/serial_gateway/include/serial_gateway.h`
- Create `F:/0GateWay/myproject/components/serial_gateway/src/serial_gateway.c`
- Modify `F:/0GateWay/myproject/components/app_common/include/app_config.h`
- Modify `F:/0GateWay/myproject/components/app_common/include/app_data.h`
- Modify `F:/0GateWay/myproject/main/main.c`
- Modify `F:/0GateWay/myproject/main/CMakeLists.txt`

- [ ] Define the newline-delimited JSON schema emitted by the gateway.
- [ ] Add a serial output helper that formats decoded frames and RSSI/SNR metadata.
- [ ] Add a temporary boot-time sample JSON output for PC parser development.
- [ ] Build and verify serial output shape before adding real LoRa.

## Milestone 4: Gateway LoRa Receive

**Files:**
- Create `F:/0GateWay/myproject/components/lora_service/CMakeLists.txt`
- Create `F:/0GateWay/myproject/components/lora_service/include/lora_service.h`
- Create `F:/0GateWay/myproject/components/lora_service/src/lora_service.c`
- Modify `F:/0GateWay/myproject/components/app_common/include/app_config.h`
- Modify `F:/0GateWay/myproject/main/main.c`
- Modify `F:/0GateWay/myproject/main/CMakeLists.txt`

- [ ] Confirm exact LoRa module, frequency, SPI pins, reset pin, DIO pin, antenna, and power supply.
- [ ] Add SPI bus init and LoRa module reset/init.
- [ ] Receive encoded sample frames.
- [ ] Log receive result, source node id, sequence number, frame length, RSSI, and SNR.
- [ ] Decode valid frames and forward JSON over USB serial.
- [ ] Build, flash, and verify receive path with one node or a known-good LoRa sender nearby.

## Milestone 5: Sensor Node Firmware

**Files:**
- Create a separate firmware project later, for example `F:/0GateWay/node_firmware`
- Reuse the gateway `frame_codec` protocol definitions where practical

- [ ] Confirm exact sensor module and wiring.
- [ ] Add I2C sensor acquisition.
- [ ] Encode samples using the shared frame format.
- [ ] Transmit frames over LoRa.
- [ ] Build, flash, and verify against the gateway.

## Milestone 6: PC Serial Ingestion and Storage

**Files:**
- Create `F:/0GateWay/pc_gateway/serial_ingest.py`
- Create `F:/0GateWay/pc_gateway/storage.py`
- Create `F:/0GateWay/pc_gateway/requirements.txt`

- [ ] Create SQLite tables for nodes, readings, and ingest errors.
- [ ] Read JSON lines from the gateway serial port.
- [ ] Validate required fields and insert valid readings.
- [ ] Log malformed lines without stopping ingestion.
- [ ] Run a short 10-minute capture and inspect database row counts.

## Milestone 7: Dashboard and Alert Baseline

**Files:**
- Create `F:/0GateWay/pc_gateway/app.py`
- Create `F:/0GateWay/pc_gateway/static/index.html`

- [ ] Expose latest reading and history query endpoints.
- [ ] Show inside/outside temperature and humidity.
- [ ] Show node online/offline state based on last-seen timestamp.
- [ ] Add fixed threshold alerts for high temperature, high humidity, and offline node.
- [ ] Run the local web server and verify data updates while serial ingestion is active.

## Milestone 8: Continuous Data Collection

**Files:**
- Modify `F:/0GateWay/pc_gateway/storage.py`
- Create `F:/0GateWay/pc_gateway/export_dataset.py`
- Create `F:/0GateWay/pc_gateway/README.md`

- [ ] Export ordered CSV with node id, timestamp, inside/outside temperature, inside/outside humidity, and status.
- [ ] Add notes for power loss, sensor errors, LoRa packet loss, and manual hive events.
- [ ] Run at least a short overnight collection before moving to real hive placement.
- [ ] After 14 days of valid data, freeze the first dataset for baseline analysis.

## Milestone 9: GRU Baseline Preparation

**Files:**
- Create `F:/0GateWay/ml/prepare_dataset.py`
- Create `F:/0GateWay/ml/train_baseline.py`
- Create `F:/0GateWay/ml/train_gru.py`
- Create `F:/0GateWay/ml/evaluate.py`

- [ ] Split data by time order, not random shuffle.
- [ ] Implement persistence baseline using previous measured value.
- [ ] Train a small GRU only after enough continuous data exists.
- [ ] Report MAE/RMSE and compare against the baseline.
- [ ] Use prediction residual only as auxiliary environment/sensor warning, not disease diagnosis.

## Milestone 10: Documentation for Graduation Deliverables

**Files:**
- Create `F:/0GateWay/docs/hardware_pin_map.md`
- Create `F:/0GateWay/docs/data_protocol.md`
- Create `F:/0GateWay/docs/test_record.md`
- Update existing graduation documents under `F:/1毕设/毕设文档` only after confirming wording.

- [ ] Record board model, module model, sensor model, LoRa model, and wiring.
- [ ] Document frame format and JSON serial format.
- [ ] Record build/flash commands and ESP-IDF version.
- [ ] Record test screenshots, serial logs, and database evidence.
- [ ] Keep claims limited to prototype monitoring and short-term prediction.

---

## First Implementation Slice

Start with gateway Milestone 1 and Milestone 2 only:

1. Refactor board info logging.
2. Remove Wi-Fi scan from default firmware.
3. Add gateway-oriented data structures and frame codec.
4. Build once.

This produces a stable gateway firmware skeleton before LoRa hardware ambiguity enters the code.
