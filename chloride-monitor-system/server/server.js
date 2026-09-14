require("dotenv").config();

const express = require("express");
const cors = require("cors");
const path = require("path");
const { pool } = require("./db");
const { mqttState, startMqttClient } = require("./mqtt-client");
const { attachUser, requireAdmin, requireLogin } = require("./middleware/auth");
const authRouter = require("./routes/auth");
const usersRouter = require("./routes/users");
const samplesRouter = require("./routes/samples");
const commandsRouter = require("./routes/commands");
const uploadStatusRouter = require("./routes/upload-status");

const app = express();
const PORT = Number(process.env.PORT || 3000);

app.use(cors());
app.use(express.json({ limit: "1mb" }));
app.use(attachUser);

app.get(["/", "/index.html"], (req, res, next) => {
  if (!req.user) {
    res.redirect("/login.html");
    return;
  }
  next();
});

app.use(express.static(path.join(__dirname, "public")));

app.get("/health", requireLogin, async (req, res, next) => {
  try {
    await pool.query("SELECT 1");
    res.json({ ok: true, mqtt: mqttState() });
  } catch (error) {
    next(error);
  }
});

app.use("/api/auth", authRouter);
app.use("/api/samples", requireLogin, samplesRouter);
app.use("/api/upload-status", requireLogin, uploadStatusRouter);
app.use("/api/users", requireLogin, requireAdmin, usersRouter);
app.use("/api", requireLogin, requireAdmin, commandsRouter);

app.use((req, res) => {
  res.status(404).json({ error: "not_found" });
});

app.use((error, req, res, next) => {
  console.error(error);
  res.status(error.status || 500).json({
    error: error.code || "internal_error",
    message: error.message || "Internal server error",
  });
});

app.listen(PORT, "0.0.0.0", () => {
  console.log(`pH water server listening on http://0.0.0.0:${PORT}`);
});

startMqttClient();

const { timeoutRunningCommands } = require("./services/command-store");
setInterval(() => {
  timeoutRunningCommands(5).catch((error) => {
    console.error("Command timeout sweep failed:", error.message);
  });
}, 60_000);
