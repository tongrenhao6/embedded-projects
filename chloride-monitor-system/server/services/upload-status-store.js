const { pool, findDeviceBySn } = require("../db");
const { DATETIME_RE, appError } = require("./sample-store");

function normalizeCount(value, fieldName) {
  const number = Number.parseInt(value ?? 0, 10);
  if (!Number.isFinite(number) || number < 0) {
    throw appError(400, `invalid_${fieldName}`, `${fieldName} must be a non-negative integer`);
  }
  return number;
}

function normalizeDateTime(value, fieldName) {
  if (value == null || value === "") {
    return null;
  }

  const normalized = String(value).replace("T", " ");
  if (!DATETIME_RE.test(normalized)) {
    throw appError(400, `invalid_${fieldName}`, `${fieldName} must be YYYY-MM-DD HH:mm:ss`);
  }
  return normalized;
}

function statusDto(deviceSn, row = {}) {
  return {
    device_id: deviceSn,
    pending_count: row.pending_count ?? 0,
    failed_count: row.failed_count ?? 0,
    oldest_sample_time: row.oldest_sample_time ?? null,
    last_upload_at: row.last_upload_at ?? null,
    last_error: row.last_error ?? null,
    updated_at: row.updated_at ?? null,
  };
}

async function getUploadStatus(deviceSn) {
  if (!deviceSn || typeof deviceSn !== "string") {
    throw appError(400, "invalid_device_id", "device_id is required");
  }

  const device = await findDeviceBySn(deviceSn);
  if (!device) {
    throw appError(404, "device_not_found", `Device ${deviceSn} not found`);
  }

  const [rows] = await pool.execute(
    `SELECT
       pending_count,
       failed_count,
       oldest_sample_time,
       last_upload_at,
       last_error,
       updated_at
     FROM device_upload_status
     WHERE device_id = ?
     LIMIT 1`,
    [device.id]
  );

  return statusDto(device.device_sn, rows[0]);
}

async function updateUploadStatus(deviceSn, payload = {}) {
  const device = await findDeviceBySn(deviceSn);
  if (!device) {
    throw appError(404, "device_not_found", `Device ${deviceSn} not found`);
  }

  const pendingCount = normalizeCount(payload.pending_count, "pending_count");
  const failedCount = normalizeCount(payload.failed_count, "failed_count");
  const oldestSampleTime = normalizeDateTime(payload.oldest_sample_time, "oldest_sample_time");
  const lastUploadAt = normalizeDateTime(payload.last_upload_at, "last_upload_at");
  const lastError = payload.last_error == null || payload.last_error === ""
    ? null
    : String(payload.last_error).slice(0, 255);

  await pool.execute(
    `INSERT INTO device_upload_status
       (device_id, pending_count, failed_count, oldest_sample_time, last_upload_at, last_error)
     VALUES (?, ?, ?, ?, ?, ?)
     ON DUPLICATE KEY UPDATE
       pending_count = VALUES(pending_count),
       failed_count = VALUES(failed_count),
       oldest_sample_time = VALUES(oldest_sample_time),
       last_upload_at = VALUES(last_upload_at),
       last_error = VALUES(last_error)`,
    [device.id, pendingCount, failedCount, oldestSampleTime, lastUploadAt, lastError]
  );

  await pool.execute(
    "UPDATE devices SET last_seen_at = NOW(), status = 'active' WHERE id = ?",
    [device.id]
  );

  return statusDto(device.device_sn, {
    pending_count: pendingCount,
    failed_count: failedCount,
    oldest_sample_time: oldestSampleTime,
    last_upload_at: lastUploadAt,
    last_error: lastError,
  });
}

module.exports = {
  getUploadStatus,
  updateUploadStatus,
};
