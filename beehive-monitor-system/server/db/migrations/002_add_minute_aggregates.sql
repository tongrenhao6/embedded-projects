-- One durable minute aggregate per gateway/node. Existing sensor_readings rows are preserved.
USE beehive_gateway;

CREATE TABLE IF NOT EXISTS sensor_minute_aggregates (
  gateway_id VARCHAR(64) NOT NULL,
  node_id INT UNSIGNED NOT NULL,
  bucket_start DATETIME NOT NULL,
  sample_count INT UNSIGNED NOT NULL,
  inside_temperature_avg DECIMAL(7,2) NULL,
  inside_temperature_min DECIMAL(7,2) NULL,
  inside_temperature_max DECIMAL(7,2) NULL,
  inside_temperature_valid_count INT UNSIGNED NOT NULL,
  inside_humidity_avg DECIMAL(7,2) NULL,
  inside_humidity_min DECIMAL(7,2) NULL,
  inside_humidity_max DECIMAL(7,2) NULL,
  inside_humidity_valid_count INT UNSIGNED NOT NULL,
  outside_temperature_avg DECIMAL(7,2) NULL,
  outside_temperature_min DECIMAL(7,2) NULL,
  outside_temperature_max DECIMAL(7,2) NULL,
  outside_temperature_valid_count INT UNSIGNED NOT NULL,
  outside_humidity_avg DECIMAL(7,2) NULL,
  outside_humidity_min DECIMAL(7,2) NULL,
  outside_humidity_max DECIMAL(7,2) NULL,
  outside_humidity_valid_count INT UNSIGNED NOT NULL,
  tilt_valid_count INT UNSIGNED NOT NULL,
  tilt_alarm_count INT UNSIGNED NOT NULL,
  status_flags_or INT UNSIGNED NOT NULL,
  last_sequence_no BIGINT UNSIGNED NOT NULL,
  last_uptime_ms BIGINT UNSIGNED NOT NULL,
  last_received_at DATETIME(3) NOT NULL,
  PRIMARY KEY (gateway_id, node_id, bucket_start),
  KEY ix_minute_aggregate_gateway_node_bucket (gateway_id, node_id, bucket_start)
) ENGINE=InnoDB;
