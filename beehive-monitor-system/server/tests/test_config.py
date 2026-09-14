import pytest

from beehive_gateway.config import ConfigurationError, load_settings, load_web_settings


def test_loads_gateway_settings_from_environment() -> None:
    settings = load_settings(
        {
            "BEEHIVE_SERIAL_PORT": "COM7",
            "BEEHIVE_SERIAL_BAUD": "115200",
            "BEEHIVE_DB_HOST": "127.0.0.1",
            "BEEHIVE_DB_PORT": "3306",
            "BEEHIVE_DB_NAME": "beehive_gateway",
            "BEEHIVE_DB_USER": "beehive_app",
            "BEEHIVE_DB_PASSWORD": "not-a-real-password",
        }
    )

    assert settings.serial_port == "COM7"
    assert settings.serial_baud == 115200
    assert settings.database_name == "beehive_gateway"
    assert settings.database_user == "beehive_app"


def test_uses_safe_database_defaults() -> None:
    settings = load_settings(
        {
            "BEEHIVE_SERIAL_PORT": "COM7",
            "BEEHIVE_DB_PASSWORD": "not-a-real-password",
        }
    )

    assert settings.serial_baud == 115200
    assert settings.database_host == "127.0.0.1"
    assert settings.database_port == 3306
    assert settings.database_name == "beehive_gateway"
    assert settings.database_user == "beehive_app"


def test_requires_serial_port() -> None:
    with pytest.raises(ConfigurationError, match="BEEHIVE_SERIAL_PORT"):
        load_settings({"BEEHIVE_DB_PASSWORD": "not-a-real-password"})


def test_rejects_invalid_database_port() -> None:
    with pytest.raises(ConfigurationError, match="BEEHIVE_DB_PORT"):
        load_settings(
            {
                "BEEHIVE_SERIAL_PORT": "COM7",
                "BEEHIVE_DB_PASSWORD": "not-a-real-password",
                "BEEHIVE_DB_PORT": "invalid",
            }
        )


def test_loads_web_settings_without_serial_port() -> None:
    settings = load_web_settings(
        {
            "BEEHIVE_DB_PASSWORD": "not-a-real-password",
            "BEEHIVE_MQTT_URI": "mqtt://broker.emqx.io:1883",
        }
    )

    assert settings.serial_port == ""
    assert settings.mqtt_uri == "mqtt://broker.emqx.io:1883"
    assert settings.mqtt_client_id == "beehive_web"
