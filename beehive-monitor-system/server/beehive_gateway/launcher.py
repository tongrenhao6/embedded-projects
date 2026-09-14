"""Windows launcher that loads a local .env, starts the service, then opens the dashboard."""
from __future__ import annotations

import os
import sys
import threading
import time
import urllib.request
import webbrowser
from pathlib import Path
from typing import MutableMapping

from beehive_gateway.config import ConfigurationError, load_web_settings
from beehive_gateway.web_server import main as run_web_server


def load_env_file(path: Path, environment: MutableMapping[str, str]) -> None:
    """Load simple KEY=VALUE lines without overwriting inherited environment values."""
    if not path.is_file():
        return
    for raw_line in path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        key, value = line.split("=", 1)
        key = key.strip()
        if key:
            environment.setdefault(key, value.strip())


def _application_directory() -> Path:
    if getattr(sys, "frozen", False):
        return Path(sys.executable).resolve().parent
    return Path(__file__).resolve().parents[1]


def main() -> int:
    app_dir = _application_directory()
    env_path = app_dir / ".env"
    load_env_file(env_path, os.environ)
    try:
        settings = load_web_settings()
    except ConfigurationError as error:
        print(f"Configuration error: {error}")
        print(f"Create or update: {env_path}")
        return 2

    thread = threading.Thread(target=run_web_server, name="beehive-web", daemon=False)
    thread.start()
    url = f"http://{settings.http_host}:{settings.http_port}/"
    deadline = time.monotonic() + 20
    while time.monotonic() < deadline:
        try:
            with urllib.request.urlopen(url + "api/health", timeout=1) as response:
                if response.status == 200:
                    webbrowser.open(url)
                    break
        except OSError:
            time.sleep(0.25)
    else:
        print(f"Backend did not become ready within 20 seconds: {url}")
        return 1
    thread.join()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
