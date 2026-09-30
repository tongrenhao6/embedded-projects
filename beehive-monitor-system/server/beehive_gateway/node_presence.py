"""Node online status derived from the freshness of telemetry."""

from __future__ import annotations

from datetime import datetime
from typing import Any


NODE_STALE_AFTER_SECONDS = 10.0
NODE_OFFLINE_AFTER_SECONDS = 60.0


def get_node_presence(
    last_seen_at: datetime | None,
    *,
    now: datetime | None = None,
) -> dict[str, Any]:
    """Classify a node using the age of its most recently received telemetry."""
    if last_seen_at is None:
        return {
            "status": "unknown",
            "last_seen_at": None,
            "last_seen_age_seconds": None,
        }

    current_time = now or datetime.now(tz=last_seen_at.tzinfo)
    observed_time = last_seen_at
    if (current_time.tzinfo is None) != (observed_time.tzinfo is None):
        current_time = current_time.replace(tzinfo=None)
        observed_time = observed_time.replace(tzinfo=None)

    age_seconds = max(0.0, (current_time - observed_time).total_seconds())
    if age_seconds <= NODE_STALE_AFTER_SECONDS:
        status = "online"
    elif age_seconds <= NODE_OFFLINE_AFTER_SECONDS:
        status = "stale"
    else:
        status = "offline"

    return {
        "status": status,
        "last_seen_at": last_seen_at,
        "last_seen_age_seconds": round(age_seconds, 1),
    }
