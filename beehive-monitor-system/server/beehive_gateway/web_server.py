"""Uvicorn entry point for the HTTP API and MQTT worker."""

from __future__ import annotations

import logging

import uvicorn

from beehive_gateway.config import ConfigurationError, load_web_settings


def main() -> int:
    logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(name)s: %(message)s")
    try:
        settings = load_web_settings()
    except ConfigurationError as error:
        logging.getLogger(__name__).error("Invalid configuration: %s", error)
        return 2
    uvicorn.run(
        "beehive_gateway.api:create_app",
        factory=True,
        host=settings.http_host,
        port=settings.http_port,
        log_level="info",
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
