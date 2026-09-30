# Project Structure

This workspace is organized around the LoRa gateway. Firmware logic should not accumulate in `main`.

## Firmware

- `main/`
  - ESP-IDF application entry point.
  - Keep only startup, top-level task wiring, and high-level application flow here.
- `components/app_common/`
  - Shared gateway configuration, data structures, constants, and small common helpers.
- `components/board_support/`
  - Board bring-up helpers: flash, PSRAM, heap, reset reason, board identity, pin notes.
- `components/frame_codec/`
  - LoRa payload decode/encode, CRC, frame versioning. Keep this protocol component reusable by the future sensor-node firmware.
- `components/sensor_service/`
  - Reserved for future node-side sensor code or shared sensor abstractions. Do not wire this into the gateway firmware unless the gateway later gets local sensors.
- `components/lora_service/`
  - DX-LR22 UART receive flow and framed stream parser integration.
- `components/wifi_service/`
  - Wi-Fi Station initialization, IP event reporting, and automatic reconnect.
- `components/mqtt_payload/`
  - Host-testable MQTT telemetry topic and JSON payload formatting.
- `components/mqtt_service/`
  - ESP-IDF MQTT client, bounded RAM queue, QoS 1 telemetry, retained status, and LWT.
- `components/serial_gateway/`
  - Preserved `GWJSON ` newline-delimited JSON output for PC ingestion.

Each component should use the normal ESP-IDF layout:

```text
components/<name>/
  CMakeLists.txt
  include/<name>.h
  src/<name>.c
```

Placeholder components may keep a minimal `CMakeLists.txt` with only `INCLUDE_DIRS "include"` so ESP-IDF can scan the component tree without warnings. Add `SRCS` only when the component has real source files.

## PC Gateway

- `F:/0GateWay/pc_gateway/`
  - Python serial ingestion service, SQLite storage, local dashboard.
- `F:/0GateWay/pc_gateway/static/`
  - Browser UI files.
- `F:/0GateWay/pc_gateway/data/`
  - Local SQLite database and exported capture files.
- `F:/0GateWay/pc_gateway/tests/`
  - Host-side parser and storage tests.

## Machine Learning

- `F:/0GateWay/ml/data/`
  - Exported datasets for model training and evaluation.
- `F:/0GateWay/ml/models/`
  - Trained model files.
- `F:/0GateWay/ml/reports/`
  - Evaluation outputs and figures.

## Documentation

- `docs/hardware/`
  - Board model, module model, wiring, pin map, power notes.
- `docs/protocol/`
  - LoRa binary frame and serial JSON protocol.
- `F:/0GateWay/docs/test_records/`
  - Test records, screenshots, serial logs, and collection notes.
