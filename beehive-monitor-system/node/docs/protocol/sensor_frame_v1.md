# Sensor Frame Protocol V1

All multi-byte values use big-endian byte order. The CRC is CRC-16/CCITT-FALSE
with polynomial `0x1021`, initial value `0xFFFF`, no reflection, and no final XOR.

| Byte offset | Length | Field | Unit |
| --- | --- | --- | --- |
| 0 | 1 | Magic (`0xB1`) | - |
| 1 | 1 | Protocol version (`1`) | - |
| 2 | 1 | Message type (`1`: sensor sample) | - |
| 3 | 1 | Payload length (`20`) | bytes |
| 4 | 2 | Node ID | - |
| 6 | 4 | Sequence number | - |
| 10 | 4 | Node uptime | ms |
| 14 | 2 | Inside temperature | 0.01 C |
| 16 | 2 | Inside humidity | 0.01 %RH |
| 18 | 2 | Outside temperature | 0.01 C |
| 20 | 2 | Outside humidity | 0.01 %RH |
| 22 | 2 | Status flags | - |
| 24 | 2 | CRC-16 | - |

The frame has a fixed length of 26 bytes. A receiver must validate the header,
version, message type, payload length, and CRC before consuming any measurement.

Status flag bit 0 indicates that the inside temperature and humidity are valid.
Bit 1 indicates that the outside temperature and humidity are valid. A receiver
must ignore the corresponding values when their validity bit is clear.
Bit 2 indicates that the MPU6050 tilt reading is valid. Bit 3 indicates that the
node is currently tilted. A receiver must ignore bit 3 when bit 2 is clear.
