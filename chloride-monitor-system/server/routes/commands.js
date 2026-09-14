const express = require("express");
const {
  createCommand,
  getCommand,
  listCommands,
  markCommandRunning,
  updateCommandResult,
} = require("../services/command-store");
const { publishCommandDown } = require("../mqtt-client");

const router = express.Router();

router.post("/commands", async (req, res, next) => {
  let command = null;

  try {
    command = await createCommand(req.body || {});
    await publishCommandDown(command);
    await markCommandRunning(command.id);

    res.status(201).json({
      ok: true,
      command: {
        ...command,
        status: "running",
      },
    });
  } catch (error) {
    if (command) {
      try {
        await updateCommandResult(command.id, "failed", `mqtt publish failed: ${error.message}`);
      } catch (updateError) {
        console.error("Failed to mark command publish error:", updateError.message);
      }
      error.status = error.status || 503;
      error.code = error.code || "mqtt_publish_failed";
    }
    next(error);
  }
});

router.get("/commands", async (req, res, next) => {
  try {
    const commands = await listCommands({
      deviceId: req.query.device_id,
      limit: req.query.limit,
    });

    res.json({
      ok: true,
      count: commands.length,
      commands,
    });
  } catch (error) {
    next(error);
  }
});

router.get("/commands/:id", async (req, res, next) => {
  try {
    res.json({
      ok: true,
      command: await getCommand(req.params.id),
    });
  } catch (error) {
    next(error);
  }
});

module.exports = router;
