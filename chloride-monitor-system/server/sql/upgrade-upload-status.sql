USE water_monitor;

ALTER TABLE samples
  ADD UNIQUE KEY uk_samples_device_time (device_id, sample_time);

CREATE TABLE IF NOT EXISTS device_upload_status (
  device_id BIGINT UNSIGNED NOT NULL,
  pending_count INT UNSIGNED NOT NULL DEFAULT 0,
  failed_count INT UNSIGNED NOT NULL DEFAULT 0,
  oldest_sample_time DATETIME NULL,
  last_upload_at DATETIME NULL,
  last_error VARCHAR(255) NULL,
  updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (device_id),
  CONSTRAINT fk_device_upload_status_device_id FOREIGN KEY (device_id) REFERENCES devices (id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
