const { pool, findDeviceBySn, parseJsonField } = require("../db");

const RESULT_STATUS = new Set(["done", "failed", "cancelled"]);

function appError(status, code, message) {
  const error = new Error(message);
  error.status = status;
  error.code = code;
  return error;
}

function commandDto(row) {
  return {
    id: row.id,
    device_id: row.device_id,
    command_name: row.command_name,
    payload: parseJsonField(row.payload),
    status: row.status,
    result: row.result,
    created_at: row.created_at,
    updated_at: row.updated_at,
    started_at: row.started_at,
    finished_at: row.finished_at,
  };
}

function validateCommandId(commandId) {
  const id = Number.parseInt(commandId, 10);
  if (!Number.isFinite(id) || id <= 0) {
    throw appError(400, "invalid_command_id", "Command id must be a positive integer");
  }
  return id;
}

async function createCommand({ device_id: deviceSn, command_name: commandName, payload = null }) {
  if (!deviceSn || typeof deviceSn !== "string") {
    throw appError(400, "invalid_device_id", "device_id is required");
  }
  if (!commandName || typeof commandName !== "string") {
    throw appError(400, "invalid_command_name", "command_name is required");
  }

  const device = await findDeviceBySn(deviceSn);
  if (!device) {
    throw appError(404, "device_not_found", `Device ${deviceSn} not found`);
  }

  const [result] = await pool.execute(
    `INSERT INTO commands (device_id, command_name, payload, status)
     VALUES (?, ?, ?, 'pending')`,
    [device.id, commandName, JSON.stringify(payload)]
  );

  return {
    id: result.insertId,
    device_id: device.device_sn,
    command_name: commandName,
    payload,
    status: "pending",
  };
}

async function markCommandRunning(commandId) {
  const id = validateCommandId(commandId);
  const [result] = await pool.execute(
    "UPDATE commands SET status = 'running', started_at = COALESCE(started_at, NOW()) WHERE id = ?",
    [id]
  );

  if (result.affectedRows === 0) {
    throw appError(404, "command_not_found", `Command ${id} not found`);
  }
}

async function getCommand(commandId) {
  const id = validateCommandId(commandId);
  const [rows] = await pool.execute(
    `SELECT
       c.id,
       d.device_sn AS device_id,
       c.command_name,
       c.payload,
       c.status,
       c.result,
       c.created_at,
       c.updated_at,
       c.started_at,
       c.finished_at
     FROM commands c
     JOIN devices d ON d.id = c.device_id
     WHERE c.id = ?
     LIMIT 1`,
    [id]
  );

  if (rows.length === 0) {
    throw appError(404, "command_not_found", `Command ${id} not found`);
  }

  return commandDto(rows[0]);
}

async function listCommands({ deviceId, limit = 10 } = {}) {
  const safeLimit = Math.min(Math.max(Number.parseInt(limit, 10) || 10, 1), 50);
  const params = [];
  const where = [];

  if (deviceId) {
    where.push("d.device_sn = ?");
    params.push(deviceId);
  }

  const whereSql = where.length > 0 ? `WHERE ${where.join(" AND ")}` : "";

  const [rows] = await pool.execute(
    `SELECT
       c.id,
       d.device_sn AS device_id,
       c.command_name,
       c.payload,
       c.status,
       c.result,
       c.created_at,
       c.updated_at,
       c.started_at,
       c.finished_at
     FROM commands c
     JOIN devices d ON d.id = c.device_id
     ${whereSql}
     ORDER BY c.created_at DESC, c.id DESC
     LIMIT ${safeLimit}`,
    params
  );

  return rows.map(commandDto);
}

async function updateCommandResult(commandId, status, result = null) {
  const id = validateCommandId(commandId);

  if (!RESULT_STATUS.has(status)) {
    throw appError(400, "invalid_status", "status must be done, failed, or cancelled");
  }

  const [updateResult] = await pool.execute(
    `UPDATE commands
     SET status = ?, result = ?, finished_at = NOW()
     WHERE id = ?`,
    [status, result == null ? null : String(result), id]
  );

  if (updateResult.affectedRows === 0) {
    throw appError(404, "command_not_found", `Command ${id} not found`);
  }

  return {
    id,
    status,
    result,
  };
}

async function timeoutRunningCommands(timeoutMinutes = 5) {
  const [result] = await pool.execute(
    `UPDATE commands
     SET status = 'failed',
         result = 'command timed out after no device response',
         finished_at = NOW()
     WHERE status = 'running'
       AND started_at IS NOT NULL
       AND started_at < DATE_SUB(NOW(), INTERVAL ? MINUTE)`,
    [timeoutMinutes]
  );

  if (result.affectedRows > 0) {
    console.log(`Timed out ${result.affectedRows} running commands (${timeoutMinutes} min threshold)`);
  }

  return result.affectedRows;
}

module.exports = {
  RESULT_STATUS,
  appError,
  commandDto,
  createCommand,
  getCommand,
  listCommands,
  markCommandRunning,
  timeoutRunningCommands,
  updateCommandResult,
};
