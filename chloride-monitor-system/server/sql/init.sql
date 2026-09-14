CREATE DATABASE IF NOT EXISTS water_monitor
  DEFAULT CHARACTER SET utf8mb4
  DEFAULT COLLATE utf8mb4_unicode_ci;

USE water_monitor;

CREATE TABLE IF NOT EXISTS devices (
  id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  device_sn VARCHAR(64) NOT NULL,
  name VARCHAR(128) NOT NULL,
  location VARCHAR(255) NULL,
  status VARCHAR(32) NOT NULL DEFAULT 'active',
  last_seen_at DATETIME NULL,
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (id),
  UNIQUE KEY uk_devices_device_sn (device_sn)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS sensors (
  id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  device_id BIGINT UNSIGNED NOT NULL,
  sensor_type VARCHAR(32) NOT NULL,
  sensor_addr TINYINT UNSIGNED NULL,
  name VARCHAR(128) NOT NULL,
  unit_note VARCHAR(64) NULL,
  enabled TINYINT(1) NOT NULL DEFAULT 1,
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (id),
  KEY idx_sensors_device_id (device_id),
  CONSTRAINT fk_sensors_device_id FOREIGN KEY (device_id) REFERENCES devices (id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS samples (
  id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  device_id BIGINT UNSIGNED NOT NULL,
  sensor_id BIGINT UNSIGNED NULL,
  sample_time DATETIME NOT NULL,
  ph DECIMAL(5,2) NULL,
  temperature DECIMAL(5,1) NULL,
  source VARCHAR(32) NOT NULL DEFAULT 'realtime',
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (id),
  UNIQUE KEY uk_samples_device_time (device_id, sample_time),
  KEY idx_samples_sensor_id (sensor_id),
  CONSTRAINT fk_samples_device_id FOREIGN KEY (device_id) REFERENCES devices (id),
  CONSTRAINT fk_samples_sensor_id FOREIGN KEY (sensor_id) REFERENCES sensors (id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS sample_values (
  id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  sample_id BIGINT UNSIGNED NOT NULL,
  metric VARCHAR(64) NOT NULL,
  value DECIMAL(12,4) NOT NULL,
  unit VARCHAR(32) NULL,
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (id),
  KEY idx_sample_values_sample_id (sample_id),
  CONSTRAINT fk_sample_values_sample_id FOREIGN KEY (sample_id) REFERENCES samples (id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS commands (
  id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  device_id BIGINT UNSIGNED NOT NULL,
  command_name VARCHAR(64) NOT NULL,
  payload JSON NULL,
  status ENUM('pending','running','done','failed','cancelled') NOT NULL DEFAULT 'pending',
  result TEXT NULL,
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  started_at DATETIME NULL,
  finished_at DATETIME NULL,
  PRIMARY KEY (id),
  KEY idx_commands_device_status (device_id, status, created_at),
  CONSTRAINT fk_commands_device_id FOREIGN KEY (device_id) REFERENCES devices (id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS upload_logs (
  id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  device_id BIGINT UNSIGNED NOT NULL,
  file_name VARCHAR(128) NULL,
  line_no INT UNSIGNED NULL,
  endpoint VARCHAR(255) NULL,
  payload JSON NULL,
  result VARCHAR(64) NOT NULL,
  message TEXT NULL,
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (id),
  KEY idx_upload_logs_device_id (device_id),
  CONSTRAINT fk_upload_logs_device_id FOREIGN KEY (device_id) REFERENCES devices (id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

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

CREATE TABLE IF NOT EXISTS users (
  id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  username VARCHAR(64) NOT NULL,
  password_hash VARCHAR(255) NOT NULL,
  role ENUM('user','admin') NOT NULL DEFAULT 'user',
  display_name VARCHAR(128) NULL,
  enabled TINYINT(1) NOT NULL DEFAULT 1,
  last_login_at DATETIME NULL,
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (id),
  UNIQUE KEY uk_users_username (username)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

INSERT INTO devices (device_sn, name, location, status)
VALUES ('ph_terminal_001', 'pH water monitor 001', 'local test', 'active')
ON DUPLICATE KEY UPDATE
  name = VALUES(name),
  location = VALUES(location),
  status = VALUES(status);

INSERT INTO sensors (device_id, sensor_type, sensor_addr, name, unit_note, enabled)
SELECT id, 'ph', 3, 'pH RS485 sensor', 'pH and Celsius', 1
FROM devices
WHERE device_sn = 'ph_terminal_001'
  AND NOT EXISTS (
    SELECT 1
    FROM sensors
    WHERE sensors.device_id = devices.id
      AND sensors.sensor_type = 'ph'
      AND sensors.sensor_addr = 3
  );

INSERT INTO users (username, password_hash, role, display_name, enabled)
VALUES
  ('admin', 'pbkdf2$120000$8ae99f44ca743d3093e2e864f613962b$13ff1d355e11ece0f36f846290d709c3a2ab372bf751ca36612156f34e22ad25', 'admin', '管理员', 1),
  ('user', 'pbkdf2$120000$7e76db9eed905ae196383b3a48406f83$85f37db28f501073d019ce93932cb5eb968566b44299cb13ded87c2dbfa98557', 'user', '普通用户', 1)
ON DUPLICATE KEY UPDATE
  role = VALUES(role),
  display_name = VALUES(display_name),
  enabled = VALUES(enabled);
