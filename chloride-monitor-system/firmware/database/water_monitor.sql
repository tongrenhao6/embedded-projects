CREATE DATABASE IF NOT EXISTS water_monitor
  DEFAULT CHARACTER SET utf8mb4
  DEFAULT COLLATE utf8mb4_unicode_ci;

USE water_monitor;

CREATE TABLE IF NOT EXISTS devices (
  id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  device_sn VARCHAR(64) NOT NULL,
  name VARCHAR(100) NOT NULL,
  location VARCHAR(200) NULL,
  status VARCHAR(32) NOT NULL DEFAULT 'offline',
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  last_seen_at DATETIME NULL,
  PRIMARY KEY (id),
  UNIQUE KEY uk_devices_device_sn (device_sn)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS sensors (
  id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  device_id BIGINT UNSIGNED NOT NULL,
  sensor_type VARCHAR(32) NOT NULL,
  sensor_addr TINYINT UNSIGNED NULL,
  name VARCHAR(100) NOT NULL,
  enabled TINYINT(1) NOT NULL DEFAULT 1,
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (id),
  UNIQUE KEY uk_sensors_device_type_addr (device_id, sensor_type, sensor_addr),
  CONSTRAINT fk_sensors_device
    FOREIGN KEY (device_id) REFERENCES devices(id)
    ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS samples (
  id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  device_id BIGINT UNSIGNED NOT NULL,
  sensor_id BIGINT UNSIGNED NULL,
  sample_time DATETIME NOT NULL,
  ph DECIMAL(5,2) NULL,
  temperature DECIMAL(5,2) NULL,
  source VARCHAR(32) NOT NULL DEFAULT 'realtime',
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (id),
  KEY idx_samples_device_time (device_id, sample_time),
  KEY idx_samples_sensor_time (sensor_id, sample_time),
  CONSTRAINT fk_samples_device
    FOREIGN KEY (device_id) REFERENCES devices(id)
    ON DELETE CASCADE,
  CONSTRAINT fk_samples_sensor
    FOREIGN KEY (sensor_id) REFERENCES sensors(id)
    ON DELETE SET NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS sample_values (
  id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  sample_id BIGINT UNSIGNED NOT NULL,
  metric VARCHAR(64) NOT NULL,
  value DECIMAL(12,4) NOT NULL,
  unit VARCHAR(32) NULL,
  PRIMARY KEY (id),
  KEY idx_sample_values_sample (sample_id),
  KEY idx_sample_values_metric (metric),
  CONSTRAINT fk_sample_values_sample
    FOREIGN KEY (sample_id) REFERENCES samples(id)
    ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS upload_logs (
  id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  device_id BIGINT UNSIGNED NULL,
  file_name VARCHAR(100) NULL,
  line_no INT UNSIGNED NULL,
  topic VARCHAR(200) NULL,
  payload JSON NULL,
  result VARCHAR(32) NOT NULL,
  error_message VARCHAR(500) NULL,
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (id),
  KEY idx_upload_logs_device_time (device_id, created_at),
  KEY idx_upload_logs_file_line (file_name, line_no),
  CONSTRAINT fk_upload_logs_device
    FOREIGN KEY (device_id) REFERENCES devices(id)
    ON DELETE SET NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

INSERT INTO devices (device_sn, name, location, status)
VALUES ('ph_terminal_001', 'pH监测终端001', '未设置', 'offline')
ON DUPLICATE KEY UPDATE name = VALUES(name);

INSERT INTO sensors (device_id, sensor_type, sensor_addr, name)
SELECT id, 'ph', 3, 'pH传感器'
FROM devices
WHERE device_sn = 'ph_terminal_001'
ON DUPLICATE KEY UPDATE name = VALUES(name), enabled = 1;
