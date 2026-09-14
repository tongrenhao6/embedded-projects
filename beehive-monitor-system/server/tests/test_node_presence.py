from datetime import datetime, timedelta

from beehive_gateway.node_presence import get_node_presence


NOW = datetime(2026, 8, 24, 14, 0, 0)


def test_missing_last_seen_is_unknown() -> None:
    result = get_node_presence(None, now=NOW)

    assert result == {
        "status": "unknown",
        "last_seen_at": None,
        "last_seen_age_seconds": None,
    }


def test_recent_node_is_online() -> None:
    result = get_node_presence(NOW - timedelta(seconds=5), now=NOW)

    assert result["status"] == "online"
    assert result["last_seen_age_seconds"] == 5.0


def test_node_between_thresholds_is_stale() -> None:
    result = get_node_presence(NOW - timedelta(seconds=30), now=NOW)

    assert result["status"] == "stale"
    assert result["last_seen_age_seconds"] == 30.0


def test_node_over_offline_threshold_is_offline() -> None:
    result = get_node_presence(NOW - timedelta(seconds=61), now=NOW)

    assert result["status"] == "offline"
    assert result["last_seen_age_seconds"] == 61.0


def test_clock_skew_never_reports_negative_age() -> None:
    result = get_node_presence(NOW + timedelta(seconds=3), now=NOW)

    assert result["status"] == "online"
    assert result["last_seen_age_seconds"] == 0.0
