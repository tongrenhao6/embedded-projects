import pytest

from beehive_gateway.protocol import GatewayFrameError, parse_gateway_line


def test_parses_valid_gateway_json() -> None:
    record = parse_gateway_line(
        'GWJSON {"node_id":1,"sequence":11,"uptime_ms":23906,'
        '"inside_temperature":27.35,"inside_humidity":47.82,'
        '"outside_temperature":26.64,"outside_humidity":52.00,'
        '"tilted":true,"status_flags":15}'
    )

    assert record is not None
    assert record.node_id == 1
    assert record.sequence_no == 11
    assert record.tilted is True
    assert record.outside_humidity == 52.0


def test_preserves_null_measurements() -> None:
    record = parse_gateway_line(
        'GWJSON {"node_id":1,"sequence":12,"uptime_ms":25000,'
        '"inside_temperature":null,"inside_humidity":null,'
        '"outside_temperature":24.75,"outside_humidity":56.78,'
        '"tilted":null,"status_flags":2}'
    )

    assert record is not None
    assert record.inside_temperature is None
    assert record.inside_humidity is None
    assert record.tilted is None


def test_ignores_normal_esp_idf_log() -> None:
    assert parse_gateway_line('I (31842) beehive_app: Gateway heartbeat') is None


def test_rejects_invalid_json_after_gateway_prefix() -> None:
    with pytest.raises(GatewayFrameError, match='invalid JSON'):
        parse_gateway_line('GWJSON {not valid json}')


def test_rejects_missing_required_field() -> None:
    with pytest.raises(GatewayFrameError, match='status_flags'):
        parse_gateway_line('GWJSON {"node_id":1,"sequence":1,"uptime_ms":1}')


def test_rejects_boolean_as_numeric_identifier() -> None:
    with pytest.raises(GatewayFrameError, match='node_id'):
        parse_gateway_line(
            'GWJSON {"node_id":true,"sequence":1,"uptime_ms":1,'
            '"status_flags":7}'
        )
