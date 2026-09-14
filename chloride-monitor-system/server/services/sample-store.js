const { pool, findDeviceBySn } = require("../db");

const DATE_RE = /^\d{4}-\d{2}-\d{2}$/;
const TIME_RE = /^\d{2}:\d{2}:\d{2}$/;
const DATETIME_RE = /^\d{4}-\d{2}-\d{2}[ T]\d{2}:\d{2}:\d{2}$/;

function appError(status, code, message) {
  const error = new Error(message);
  error.status = status;
  error.code = code;
  return error;
}

function normalizeNumber(value, fieldName) {
  if (value == null || value === "") {
    return null;
  }

  const number = Number(value);
  if (!Number.isFinite(number)) {
    throw appError(400, `invalid_${fieldName}`, `${fieldName} must be a number`);
  }
  return number;
}

function normalizeSamplePayload(payload, options = {}) {
  if (!payload || typeof payload !== "object") {
    throw appError(400, "invalid_body", "JSON body is required");
  }

  const deviceId = payload.device_id || options.deviceId;
  if (!deviceId || typeof deviceId !== "string") {
    throw appError(400, "invalid_device_id", "device_id is required");
  }

  let date = payload.date;
  let time = payload.time;
  let sampleTime = payload.sample_time;

  if (sampleTime) {
    sampleTime = String(sampleTime).replace("T", " ");
    if (!DATETIME_RE.test(sampleTime)) {
      throw appError(400, "invalid_sample_time", "sample_time must be YYYY-MM-DD HH:mm:ss");
    }
    date = sampleTime.slice(0, 10);
    time = sampleTime.slice(11, 19);
  } else {
    if (!DATE_RE.test(date || "")) {
      throw appError(400, "invalid_date", "date must be YYYY-MM-DD");
    }
    if (!TIME_RE.test(time || "")) {
      throw appError(400, "invalid_time", "time must be HH:mm:ss");
    }
    sampleTime = `${date} ${time}`;
  }

  const readings = payload.readings || {};
  const ph = normalizeNumber(
    payload.ph ?? readings.ph?.value,
    "ph"
  );
  const temperature = normalizeNumber(
    payload.temperature ?? readings.temperature?.value,
    "temperature"
  );

  return {
    deviceId,
    date,
    time,
    sampleTime,
    ph,
    temperature,
    source: payload.source || options.source || "mqtt",
    readings,
  };
}

async function saveSample(payload, options = {}) {
  const sample = normalizeSamplePayload(payload, options);
  const connection = await pool.getConnection();

  try {
    await connection.beginTransaction();

    const device = await findDeviceBySn(sample.deviceId, connection);
    if (!device) {
      throw appError(404, "device_not_found", `Device ${sample.deviceId} not found`);
    }

    const [sensorRows] = await connection.execute(
      `SELECT id
       FROM sensors
       WHERE device_id = ? AND sensor_type = 'ph' AND enabled = 1
       ORDER BY id ASC
       LIMIT 1`,
      [device.id]
    );

    await connection.execute(
      `INSERT INTO samples
        (device_id, sensor_id, sample_time, ph, temperature, source)
       VALUES (?, ?, ?, ?, ?, ?)
       ON DUPLICATE KEY UPDATE
         sensor_id = VALUES(sensor_id),
         ph = VALUES(ph),
         temperature = VALUES(temperature),
         source = VALUES(source)`,
      [
        device.id,
        sensorRows[0]?.id || null,
        sample.sampleTime,
        sample.ph,
        sample.temperature,
        sample.source,
      ]
    );
    const [sampleRows] = await connection.execute(
      `SELECT id
       FROM samples
       WHERE device_id = ? AND sample_time = ?
       LIMIT 1`,
      [device.id, sample.sampleTime]
    );
    const sampleId = sampleRows[0].id;

    await connection.execute("DELETE FROM sample_values WHERE sample_id = ?", [sampleId]);
    await insertSampleValues(connection, sampleId, sample.readings, sample);

    await connection.execute(
      "UPDATE devices SET last_seen_at = NOW(), status = 'active' WHERE id = ?",
      [device.id]
    );

    await connection.commit();

    return {
      id: sampleId,
      device_id: device.device_sn,
      date: sample.date,
      time: sample.time,
      ph: sample.ph,
      temperature: sample.temperature,
      source: sample.source,
    };
  } catch (error) {
    await connection.rollback();
    throw error;
  } finally {
    connection.release();
  }
}

async function insertSampleValues(connection, sampleId, readings, sample) {
  const values = [];

  if (readings && typeof readings === "object") {
    for (const [metric, reading] of Object.entries(readings)) {
      const value = normalizeNumber(
        reading && typeof reading === "object" ? reading.value : reading,
        metric
      );
      if (value == null) {
        continue;
      }
      values.push([
        sampleId,
        metric,
        value,
        reading && typeof reading === "object" ? reading.unit || null : null,
      ]);
    }
  } else {
    if (sample.ph != null) {
      values.push([sampleId, "ph", sample.ph, "pH"]);
    }
    if (sample.temperature != null) {
      values.push([sampleId, "temperature", sample.temperature, "C"]);
    }
  }

  for (const value of values) {
    await connection.execute(
      "INSERT INTO sample_values (sample_id, metric, value, unit) VALUES (?, ?, ?, ?)",
      value
    );
  }
}

module.exports = {
  DATE_RE,
  TIME_RE,
  DATETIME_RE,
  appError,
  normalizeSamplePayload,
  saveSample,
};
