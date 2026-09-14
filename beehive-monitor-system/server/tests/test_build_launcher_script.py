from pathlib import Path


def test_launcher_build_excludes_conflicting_qt_bindings_and_stops_on_failure() -> None:
    script = Path("scripts/build_launcher.ps1").read_text(encoding="utf-8")

    assert "--exclude-module PyQt5" in script
    assert "--exclude-module PySide6" in script
    assert "--hidden-import beehive_gateway.api" in script
    assert "--hidden-import mysql.connector.locales.eng.client_error" in script
    assert "if ($LASTEXITCODE -ne 0)" in script
    assert "throw \"PyInstaller build failed.\"" in script


def test_launcher_environment_template_explains_exe_sidecar_file() -> None:
    template = Path(".env.example").read_text(encoding="utf-8")

    assert "Copy this file as .env beside BeehiveMonitor.exe" in template
