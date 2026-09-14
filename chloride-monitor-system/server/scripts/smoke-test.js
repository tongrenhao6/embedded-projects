const { spawn } = require("child_process");

const baseUrl = "http://127.0.0.1:3000";

function wait(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

async function requestJson(pathname, options) {
  const response = await fetch(`${baseUrl}${pathname}`, options);
  const body = await response.json();
  if (!response.ok) {
    throw new Error(`${pathname} failed: HTTP ${response.status} ${JSON.stringify(body)}`);
  }
  return body;
}

async function requestText(pathname) {
  const response = await fetch(`${baseUrl}${pathname}`);
  const text = await response.text();
  if (!response.ok) {
    throw new Error(`${pathname} failed: HTTP ${response.status}`);
  }
  return text;
}

async function main() {
  const child = spawn("F:/node.exe", ["server.js"], {
    cwd: "E:/ph-water-server",
    env: {
      ...process.env,
      Path: process.env.Path || process.env.PATH,
      PATH: undefined,
    },
    stdio: ["ignore", "pipe", "pipe"],
  });

  let stderr = "";
  child.stderr.on("data", (chunk) => {
    stderr += chunk.toString();
  });

  try {
    await wait(1500);

    const health = await requestJson("/health");
    const home = await requestText("/");
    const samples = await requestJson(
      "/api/samples?device_id=ph_terminal_001&limit=5&date=2026-05-04"
    );
    const commands = await requestJson("/api/commands?device_id=ph_terminal_001&limit=5");

    console.log(JSON.stringify({
      health,
      homeContainsTitle: home.includes("pH 水质监测平台"),
      sampleCount: samples.count,
      commandCount: commands.count,
    }, null, 2));
  } finally {
    child.kill();
    await wait(300);
    if (stderr.trim()) {
      console.error(stderr.trim());
    }
  }
}

main().catch((error) => {
  console.error(error);
  process.exit(1);
});
