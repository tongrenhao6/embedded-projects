# Node Firmware Project Structure

This ESP-IDF project is the sensor-node firmware for the beehive monitoring system.
It is maintained separately from the gateway firmware at `F:\0GateWay\myproject`.

## Source layout

- `main/`: Application entry point and service startup only.
- `components/app_common/`: Node identity, sampling policy, shared data types, and common helpers.
- `components/board_support/`: Board-specific pin mapping, power control, and peripheral setup.
- `components/frame_codec/`: Shared sensor-frame encoding, decoding, and CRC validation. The gateway must follow the same protocol.
- `components/sensor_service/`: Sensor initialization, sampling, validation, and measurement conversion.
- `components/lora_service/`: LoRa radio initialization and frame transmission.

## Documentation layout

- `docs/hardware/`: Node board, sensor, LoRa module, wiring, and power records.
- `docs/protocol/`: Wire-format definition and compatibility notes with the gateway.
- `docs/plans/`: Approved implementation plans.
- `docs/test_records/`: Build, flash, radio-link, and field-test records.

No external GPIO assignment, radio configuration, sensor driver, partition table, or SDK configuration is added until the node hardware and operating requirements are confirmed.
