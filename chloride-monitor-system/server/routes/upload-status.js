const express = require("express");
const { getUploadStatus } = require("../services/upload-status-store");

const router = express.Router();

router.get("/", async (req, res, next) => {
  try {
    res.json({
      ok: true,
      status: await getUploadStatus(req.query.device_id),
    });
  } catch (error) {
    next(error);
  }
});

module.exports = router;
