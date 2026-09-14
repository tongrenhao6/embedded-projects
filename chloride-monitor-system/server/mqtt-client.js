const mqtt = require("mqtt");
const { pool } = require("./db");
const { saveSample } = require("./services/sample-store");
const { getCommand, markCommandRunning, updateCommandResult } = require("./services/command-store");
const { updateUploadStatus } = require("./services/upload-status-store");

const mqttUrl = process.env.MQTT_URL;
const baseTopic = (process.env.MQTT_BASE_TOPIC || "phwater/v1").replace(/\/+$/, "");

let client = null;
let connected = false;

function topicFor(deviceId, suffix) {
  return `${baseTopic}/devices/${deviceId}/${suffix}`;
}

function parseDeviceTopic(topic) {
  const prefix = `${baseTopic}/devices/`;
  if (!topic.startsWith(prefix)) {
    return null;
  }

  const rest = topic.slice(prefix.length);
  const parts = rest.split("/");
  if (parts.length < 2) {
    return null;
  }

  return {
    deviceId: parts[0],
    type: parts.slice(1).join("/"),
  };
}

function parseJsonPayload(message) {
  const text = message.toString("utf8").trim();
  if (!text) {
    return {};
  }

  try {
    return JSON.parse(text);
  } catch (error) {
    error.message = `Invalid MQTT JSON payload: ${error.message}`;
    throw error;
  }
}

function startMqttClient() {
  if (client) {
    return client;
  }

  if (!mqttUrl) {
    console.warn("MQTT_URL is not configured; MQTT client disabled");
    return null;
  }

  client = mqtt.connect(mqttUrl, {
    clientId: process.env.MQTT_CLIENT_ID || "ph_water_server",
    username: process.env.MQTT_USERNAME || undefined,
    password: process.env.MQTT_PASSWORD || undefined,
    clean: true,
    keepalive: 30,
    reconnectPeriod: 3000,
    connectTimeout: 10000,
  });

  client.on("connect", () => {
    connected = true;
    console.log(`MQTT connected: ${mqttUrl}`);
    subscribeTopics();
  });

  client.on("reconnect", () => {
    console.log("MQTT reconnecting");
  });

  client.on("close", () => {
    connected = false;
    console.warn("MQTT disconnected");
  });

  client.on("error", (error) => {
    connected = false;
    console.error("MQTT error:", error.message);
  });

  client.on("message", (topic, message) => {
    handleMessage(topic, message).catch((error) => {
      console.error(`MQTT message handling failed for ${topic}:`, error.message);
    });
  });

  return client;
}

function subscribeTopics() {
  const topics = [
    topicFor("+", "telemetry"),
    topicFor("+", "status"),
    topicFor("+", "commands/up"),
    topicFor("+", "upload/status"),
  ];

  client.subscribe(topics, { qos: 1 }, (error) => {
    if (error) {
      console.error("MQTT subscribe failed:", error.message);
      return;
    }
    console.log(`MQTT subscribed: ${topics.join(", ")}`);
  });
}

async function handleMessage(topic, message) {
  const parsedTopic = parseDeviceTopic(topic);
  if (!parsedTopic) {
    return;
  }

  if (parsedTopic.type === "telemetry") {
    await handleTelemetry(parsedTopic.deviceId, message);
    return;
  }

  if (parsedTopic.type === "status") {
    await handleStatus(parsedTopic.deviceId, message);
    return;
  }

  if (parsedTopic.type === "commands/up") {
    await handleCommandUp(parsedTopic.deviceId, message);
    return;
  }

  if (parsedTopic.type === "upload/status") {
    await handleUploadStatus(parsedTopic.deviceId, message);
  }
}

async function handleTelemetry(deviceId, message) {
  const payload = parseJsonPayload(message);
  const sample = await saveSample(payload, {
    deviceId,
    source: payload.source || "mqtt",
  });
  console.log(`MQTT telemetry saved: device=${sample.device_id} sample=${sample.id}`);
}

async function handleStatus(deviceId, message) {
  const text = message.toString("utf8").trim();
  let status = text || "active";

  if (text.startsWith("{")) {
    const payload = parseJsonPayload(message);
    status = payload.status || payload.state || status;
  }

  if (status === "online") {
    status = "active";
  }

  await pool.execute(
    "UPDATE devices SET status = ?, last_seen_at = NOW() WHERE device_sn = ?",
    [String(status).slice(0, 32), deviceId]
  );
  console.log(`MQTT status updated: device=${deviceId} status=${status}`);
}

async function handleCommandUp(deviceId, message) {
  const payload = parseJsonPayload(message);
  const commandId = payload.command_id || payload.id;
  const status = payload.status;
  const result = payload.result || null;

  if (!commandId) {
    throw new Error("commands/up missing command_id");
  }

  if (status === "running") {
    await markCommandRunning(commandId);
  } else {
    await updateCommandResult(commandId, status, result);
  }

  console.log(`MQTT command result saved: device=${deviceId} command=${commandId} status=${status}`);
}

async function handleUploadStatus(deviceId, message) {
  const payload = parseJsonPayload(message);
  const status = await updateUploadStatus(deviceId, payload);
  console.log(
    `MQTT upload status saved: device=${status.device_id} pending=${status.pending_count} failed=${status.failed_count}`
  );
}

function publishCommandDown(command) {
  return new Promise((resolve, reject) => {
    if (!client || !connected) {
      reject(new Error("MQTT client is not connected"));
      return;
    }

    const payload = {
      command_id: command.id,
      command_name: command.command_name,
      payload: command.payload || {},
    };

    client.publish(
      topicFor(command.device_id, "commands/down"),
      JSON.stringify(payload),
      { qos: 1, retain: false },
      (error) => {
        if (error) {
          reject(error);
          return;
        }
        console.log(`MQTT command published: device=${command.device_id} command=${command.id}`);
        resolve();
      }
    );
  });
}

function mqttState() {
  return {
    enabled: Boolean(client),
    connected,
    url: mqttUrl || null,
    base_topic: baseTopic,
  };
}

module.exports = {
  mqttState,
  publishCommandDown,
  startMqttClient,
};
