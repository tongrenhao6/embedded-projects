const express = require("express");
const { pool } = require("../db");
const { DATE_RE, appError } = require("../services/sample-store");

const router = express.Router();

function parseLimit(value) {
  const limit = Number.parseInt(value || "50", 10);
  if (!Number.isFinite(limit) || limit <= 0) {
    return 50;
  }
  return Math.min(limit, 200);
}

function parsePage(value) {
  const page = Number.parseInt(value || "1", 10);
  if (!Number.isFinite(page) || page <= 0) {
    return 1;
  }
  return page;
}

function validateDateQuery(value, name) {
  if (value != null && value !== "" && !DATE_RE.test(value)) {
    throw appError(400, "invalid_date", `${name} must be YYYY-MM-DD`);
  }
}

router.get("/", async (req, res, next) => {
  try {
    const limit = parseLimit(req.query.limit);
    const page = parsePage(req.query.page);
    const offset = (page - 1) * limit;
    const { device_id: deviceSn, date, start_date: startDate, end_date: endDate } = req.query;
    validateDateQuery(date, "date");
    validateDateQuery(startDate, "start_date");
    validateDateQuery(endDate, "end_date");

    const where = [];
    const params = [];

    if (deviceSn) {
      where.push("d.device_sn = ?");
      params.push(deviceSn);
    }

    if (date) {
      where.push("s.sample_time >= ? AND s.sample_time < DATE_ADD(?, INTERVAL 1 DAY)");
      params.push(date, date);
    } else {
      if (startDate) {
        where.push("s.sample_time >= ?");
        params.push(startDate);
      }
      if (endDate) {
        where.push("s.sample_time < DATE_ADD(?, INTERVAL 1 DAY)");
        params.push(endDate);
      }
    }

    const whereSql = where.length > 0 ? `WHERE ${where.join(" AND ")}` : "";

    const [countRows] = await pool.execute(
      `SELECT COUNT(*) AS total
       FROM samples s
       JOIN devices d ON d.id = s.device_id
       ${whereSql}`,
      params
    );
    const total = Number(countRows[0]?.total || 0);

    const [rows] = await pool.execute(
      `SELECT
         s.id,
         d.device_sn AS device_id,
         DATE_FORMAT(s.sample_time, '%Y-%m-%d') AS date,
         DATE_FORMAT(s.sample_time, '%H:%i:%s') AS time,
         s.sample_time,
         s.ph,
         s.temperature,
         s.source,
         s.created_at
       FROM samples s
       JOIN devices d ON d.id = s.device_id
       ${whereSql}
       ORDER BY s.sample_time DESC, s.id DESC
       LIMIT ${limit} OFFSET ${offset}`,
      params
    );
    const totalPages = Math.max(Math.ceil(total / limit), 1);

    res.json({
      ok: true,
      count: rows.length,
      total,
      page,
      limit,
      total_pages: totalPages,
      samples: rows,
    });
  } catch (error) {
    next(error);
  }
});

module.exports = router;
