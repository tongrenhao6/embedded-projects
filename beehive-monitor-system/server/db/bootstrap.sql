-- Beehive gateway database bootstrap.
-- Run this file from a MySQL 8.4 administrator session.

CREATE DATABASE IF NOT EXISTS beehive_gateway
  CHARACTER SET utf8mb4
  COLLATE utf8mb4_unicode_ci;

USE beehive_gateway;

CREATE TABLE IF NOT EXISTS gateways (
  gateway_id VARCHAR(64) NOT NULL,
  status VARCHAR(16) NOT NULL,
  last_seen_at TIMESTAMP(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
  updated_at TIMESTAMP(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3)
    ON UPDATE CURRENT_TIMESTAMP(3),
  PRIMARY KEY (gateway_id),
  KEY ix_gateways_status (status)
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS sensor_readings (
  id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  received_at TIMESTAMP(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
  gateway_id VARCHAR(64) NOT NULL DEFAULT 'legacy',
  node_id INT UNSIGNED NOT NULL,
  sequence_no BIGINT UNSIGNED NOT NULL,
  uptime_ms BIGINT UNSIGNED NOT NULL,
  inside_temperature DECIMAL(7,2) NULL,
  inside_humidity DECIMAL(7,2) NULL,
  outside_temperature DECIMAL(7,2) NULL,
  outside_humidity DECIMAL(7,2) NULL,
  tilted BOOLEAN NULL,
  status_flags INT UNSIGNED NOT NULL,
  PRIMARY KEY (id),
  UNIQUE KEY uq_sensor_readings_gateway_node_sequence (gateway_id, node_id, sequence_no),
  KEY ix_sensor_readings_received_at (received_at),
  KEY ix_sensor_readings_gateway_node_received (gateway_id, node_id, received_at)
) ENGINE=InnoDB;

-- Create the application account separately after choosing its password.
-- Replace CHANGE_ME before running these statements, or run them manually:
-- CREATE USER IF NOT EXISTS 'beehive_app'@'localhost'
--   IDENTIFIED BY 'CHANGE_ME';
-- CREATE USER IF NOT EXISTS 'beehive_app'@'127.0.0.1'
--   IDENTIFIED BY 'CHANGE_ME';
-- GRANT SELECT, INSERT, UPDATE, DELETE, CREATE, ALTER, INDEX
--   ON beehive_gateway.* TO 'beehive_app'@'localhost';
-- GRANT SELECT, INSERT, UPDATE, DELETE, CREATE, ALTER, INDEX
--   ON beehive_gateway.* TO 'beehive_app'@'127.0.0.1';
-- FLUSH PRIVILEGES;
