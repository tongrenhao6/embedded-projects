# Gateway MQTT Protocol v1

The gateway publishes only successfully decoded and CRC-checked LoRa sensor frames.
MQTT configuration is provided through the `Beehive Gateway` menuconfig section.

## Configuration

The following Kconfig values are available:

- `CONFIG_BEEHIVE_WIFI_SSID`
- `CONFIG_BEEHIVE_WIFI_PASSWORD`
- `CONFIG_BEEHIVE_GATEWAY_ID`
- `CONFIG_BEEHIVE_MQTT_URI`
- `CONFIG_BEEHIVE_MQTT_USERNAME`
- `CONFIG_BEEHIVE_MQTT_PASSWORD`
- `CONFIG_BEEHIVE_MQTT_CLIENT_ID`
- `CONFIG_BEEHIVE_MQTT_TOPIC_PREFIX`

The default broker URI is `mqtt://120.27.235.176:1883`. The URI can later be changed to
`mqtts://...`; certificate verification is not configured by this prototype yet.
Credentials are never written to logs.

## Topics

For gateway `gw-001`, node `1`, and topic prefix `beehive/v1`:

- Telemetry: `beehive/v1/gateways/gw-001/nodes/1/telemetry`, QoS 1, retain 0
- Status: `beehive/v1/gateways/gw-001/status`, QoS 1, retain 1

The MQTT Last Will message is `offline`. After a successful connection, the gateway
publishes `online` to the retained status topic. The client uses a clean session,
30-second MQTT keepalive, 10-second network timeout, and 3-second reconnect interval.

## Telemetry Payload

```json
{
  "schema_version": 1,
  "gateway_id": "gw-001",
  "node_id": 1,
  "sequence": 11,
  "uptime_ms": 23906,
  "inside_temperature": 27.35,
  "inside_humidity": 47.82,
  "outside_temperature": 26.64,
  "outside_humidity": 52.00,
  "tilted": true,
  "status_flags": 15
}
```

Temperature is in degrees Celsius and humidity is in percent. Measurement fields are
`null` when their corresponding validity flag is not set. `node_id`, `sequence`,
`uptime_ms`, and `status_flags` are always present.

When Wi-Fi or MQTT is disconnected, LoRa reception continues. Up to eight decoded
samples are held in RAM for a later MQTT connection; samples beyond that bound are
dropped and counted. No offline samples are written to Flash.
