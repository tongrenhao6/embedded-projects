from __future__ import annotations

from beehive_gateway.launcher import load_env_file


def test_load_env_file_sets_missing_values_without_overriding_process_env(tmp_path) -> None:
    env_file = tmp_path / ".env"
    env_file.write_text("BEEHIVE_DB_PASSWORD=local-secret\n# comment\nBEEHIVE_HTTP_PORT=8123\n", encoding="utf-8")
    environment = {"BEEHIVE_HTTP_PORT": "8000"}

    load_env_file(env_file, environment)

    assert environment["BEEHIVE_DB_PASSWORD"] == "local-secret"
    assert environment["BEEHIVE_HTTP_PORT"] == "8000"
