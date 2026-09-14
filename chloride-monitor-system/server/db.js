require("dotenv").config();

const mysql = require("mysql2/promise");

const pool = mysql.createPool({
  host: process.env.DB_HOST,
  port: Number(process.env.DB_PORT || 3306),
  user: process.env.DB_USER,
  password: process.env.DB_PASSWORD,
  database: process.env.DB_NAME,
  waitForConnections: true,
  connectionLimit: 10,
  queueLimit: 0,
  decimalNumbers: true,
  dateStrings: true,
});

async function findDeviceBySn(deviceSn, connection = pool) {
  const [rows] = await connection.execute(
    "SELECT id, device_sn, name, status FROM devices WHERE device_sn = ? LIMIT 1",
    [deviceSn]
  );
  return rows[0] || null;
}

function parseJsonField(value) {
  if (value == null || typeof value !== "string") {
    return value;
  }

  try {
    return JSON.parse(value);
  } catch {
    return value;
  }
}

module.exports = {
  pool,
  findDeviceBySn,
  parseJsonField,
};
