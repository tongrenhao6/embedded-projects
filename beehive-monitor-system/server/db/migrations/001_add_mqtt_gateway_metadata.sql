-- Run once against an existing beehive_gateway database created by the serial-only schema.
-- This migration is intentionally separate from bootstrap.sql because the old unique key
-- must be removed before readings from multiple gateways can coexist.

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

ALTER TABLE sensor_readings
  ADD COLUMN gateway_id VARCHAR(64) NOT NULL DEFAULT 'legacy' AFTER received_at;

ALTER TABLE sensor_readings
  DROP INDEX uq_sensor_readings_node_sequence,
  ADD UNIQUE KEY uq_sensor_readings_gateway_node_sequence (gateway_id, node_id, sequence_no),
  ADD KEY ix_sensor_readings_gateway_node_received (gateway_id, node_id, received_at);
