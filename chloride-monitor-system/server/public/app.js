const DEVICE_ID = "ph_terminal_001";
const DEFAULT_SAMPLE_LIMIT = 50;
const AUTO_REFRESH_MS = 5000;
const COMMAND_REFRESH_MS = 3000;

const els = {
  statusDot: document.getElementById("statusDot"),
  statusText: document.getElementById("statusText"),
  mqttDot: document.getElementById("mqttDot"),
  mqttText: document.getElementById("mqttText"),
  uploadDot: document.getElementById("uploadDot"),
  uploadStatusText: document.getElementById("uploadStatusText"),
  mqttBroker: document.getElementById("mqttBroker"),
  autoRefreshText: document.getElementById("autoRefreshText"),
  latestPh: document.getElementById("latestPh"),
  latestTemp: document.getElementById("latestTemp"),
  latestTime: document.getElementById("latestTime"),
  latestSource: document.getElementById("latestSource"),
  pendingUploadCount: document.getElementById("pendingUploadCount"),
  failedUploadCount: document.getElementById("failedUploadCount"),
  oldestPendingTime: document.getElementById("oldestPendingTime"),
  lastUploadError: document.getElementById("lastUploadError"),
  sampleRows: document.getElementById("sampleRows"),
  rowCount: document.getElementById("rowCount"),
  pageInfo: document.getElementById("pageInfo"),
  prevPageBtn: document.getElementById("prevPageBtn"),
  nextPageBtn: document.getElementById("nextPageBtn"),
  chartHint: document.getElementById("chartHint"),
  chartPrevPageBtn: document.getElementById("chartPrevPageBtn"),
  chartNextPageBtn: document.getElementById("chartNextPageBtn"),
  chart: document.getElementById("trendChart"),
  chartFrame: document.getElementById("chartFrame"),
  fullscreenChartBtn: document.getElementById("fullscreenChartBtn"),
  singleDate: document.getElementById("singleDate"),
  startDate: document.getElementById("startDate"),
  endDate: document.getElementById("endDate"),
  sampleLimit: document.getElementById("sampleLimit"),
  queryBtn: document.getElementById("queryBtn"),
  resetBtn: document.getElementById("resetBtn"),
  refreshBtn: document.getElementById("refreshBtn"),
  commandMonth: document.getElementById("commandMonth"),
  uploadPendingBtn: document.getElementById("uploadPendingBtn"),
  commandResult: document.getElementById("commandResult"),
  commandRows: document.getElementById("commandRows"),
  commandCount: document.getElementById("commandCount"),
  userForm: document.getElementById("userForm"),
  userFormMessage: document.getElementById("userFormMessage"),
  userRows: document.getElementById("userRows"),
  userCount: document.getElementById("userCount"),
  profileForm: document.getElementById("profileForm"),
  profileMessage: document.getElementById("profileMessage"),
  uploadAlert: document.getElementById("uploadAlert"),
  uploadAlertTitle: document.getElementById("uploadAlertTitle"),
  uploadAlertMessage: document.getElementById("uploadAlertMessage"),
  alertUploadBtn: document.getElementById("alertUploadBtn"),
  alertLaterBtn: document.getElementById("alertLaterBtn"),
  currentUser: document.getElementById("currentUser"),
  logoutBtn: document.getElementById("logoutBtn"),
  navTabs: document.querySelectorAll("[data-page-target]"),
  pageSections: document.querySelectorAll("[data-page-section]"),
};

let currentUser = null;
let currentSamples = [];
let currentSamplePage = 1;
let sampleTotalPages = 1;
let sampleTotalCount = 0;
let refreshTimer = null;
let commandTimer = null;
let isLoading = false;
let activePage = "dashboard";
let chartHitPoints = [];
let selectedChartSampleIndex = null;
let isChartFullscreen = false;
let lastUploadAlertKey = "";
let uploadAlertDismissedKey = "";
let commandMonthTouched = false;

function setStatus(online, text) {
  els.statusDot.classList.toggle("online", online);
  els.statusDot.classList.toggle("offline", !online);
  els.statusText.textContent = text;
}

function setMqttStatus(state, text, brokerText = "") {
  els.mqttDot.classList.toggle("online", state === "online");
  els.mqttDot.classList.toggle("offline", state === "offline");
  els.mqttDot.classList.toggle("warning", state === "warning");
  els.mqttText.textContent = text;
  els.mqttBroker.textContent = brokerText || "Broker --";
}

function setUploadStatusIndicator(state, text) {
  els.uploadDot.classList.toggle("online", state === "online");
  els.uploadDot.classList.toggle("offline", state === "offline");
  els.uploadDot.classList.toggle("warning", state === "warning");
  els.uploadStatusText.textContent = text;
}

function escapeHtml(value) {
  return String(value ?? "")
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/"/g, "&quot;")
    .replace(/'/g, "&#39;");
}

function formatNumber(value, digits) {
  if (value == null || value === "") {
    return "--";
  }
  const number = Number(value);
  return Number.isFinite(number) ? number.toFixed(digits) : "--";
}

function isAdmin() {
  return currentUser?.role === "admin";
}

async function apiFetch(url, options) {
  const response = await fetch(url, options);
  if (response.status === 401) {
    window.location.replace("/login.html");
    throw new Error("请先登录");
  }
  return response;
}

function applyUser(user) {
  currentUser = user;
  document.body.classList.toggle("role-admin", isAdmin());
  document.body.classList.toggle("role-user", !isAdmin());
  els.currentUser.textContent = `${user.display_name || user.username}（${isAdmin() ? "管理员" : "普通用户"}）`;
  els.profileForm.username.value = user.username || "";
  els.profileForm.display_name.value = user.display_name || "";

  document.querySelectorAll(".admin-only").forEach((element) => {
    element.hidden = !isAdmin();
  });
}

function buildSampleUrl() {
  const params = new URLSearchParams({
    device_id: DEVICE_ID,
    limit: els.sampleLimit.value || String(DEFAULT_SAMPLE_LIMIT),
    page: String(currentSamplePage),
  });

  if (els.singleDate.value) {
    params.set("date", els.singleDate.value);
  } else {
    if (els.startDate.value) {
      params.set("start_date", els.startDate.value);
    }
    if (els.endDate.value) {
      params.set("end_date", els.endDate.value);
    }
  }

  return `/api/samples?${params.toString()}`;
}

function buildUploadStatusUrl() {
  const params = new URLSearchParams({
    device_id: DEVICE_ID,
  });
  return `/api/upload-status?${params.toString()}`;
}

function monthFromDateTime(value) {
  const text = String(value || "").trim();
  const match = text.match(/^(\d{4}-\d{2})-\d{2}/);
  return match ? match[1] : "";
}

function renderOverview(samples) {
  const latest = samples[0];
  if (!latest) {
    els.latestPh.textContent = "--";
    els.latestTemp.textContent = "--";
    els.latestTime.textContent = "--";
    els.latestSource.textContent = "--";
    return;
  }

  els.latestPh.textContent = formatNumber(latest.ph, 2);
  els.latestTemp.textContent = `${formatNumber(latest.temperature, 1)} ℃`;
  els.latestTime.textContent = `${latest.date || "--"} ${latest.time || ""}`.trim();
  els.latestSource.textContent = latest.source || "--";
}

function uploadAlertKey(status) {
  return [
    status.pending_count || 0,
    status.failed_count || 0,
    status.oldest_sample_time || "",
    status.last_error || "",
  ].join("|");
}

function renderUploadStatus(status) {
  const pendingCount = Number(status.pending_count || 0);
  const failedCount = Number(status.failed_count || 0);
  const lastError = String(status.last_error || "").trim();
  const hasBacklog = pendingCount > 0 || failedCount > 0;
  const hasError = lastError.length > 0;
  const oldestPendingMonth = monthFromDateTime(status.oldest_sample_time);

  els.pendingUploadCount.textContent = String(pendingCount);
  els.failedUploadCount.textContent = String(failedCount);
  els.oldestPendingTime.textContent = status.oldest_sample_time || "--";
  els.lastUploadError.textContent = lastError || "--";
  if (!commandMonthTouched && oldestPendingMonth) {
    els.commandMonth.value = oldestPendingMonth;
  }

  if (hasBacklog || hasError) {
    setUploadStatusIndicator("warning", `未上传 ${pendingCount} 条 / 失败 ${failedCount} 条`);
  } else {
    setUploadStatusIndicator("online", "未上传 0 条");
    hideUploadAlert();
  }
}

function showUploadAlert(status) {
  const pendingCount = Number(status.pending_count || 0);
  const failedCount = Number(status.failed_count || 0);
  const lastError = String(status.last_error || "").trim();
  const hasBacklog = pendingCount > 0 || failedCount > 0;
  const key = uploadAlertKey(status);
  if (!key || key === uploadAlertDismissedKey || key === lastUploadAlertKey) {
    return;
  }

  lastUploadAlertKey = key;
  els.uploadAlertTitle.textContent = hasBacklog ? "发现未上传数据" : "设备存储错误";
  els.uploadAlertMessage.textContent = [
    hasBacklog ? `当前设备还有 ${pendingCount} 条数据未上传。` : "",
    hasBacklog ? `失败 ${failedCount} 条。` : "",
    status.oldest_sample_time ? `最早时间：${status.oldest_sample_time}。` : "",
    lastError ? `最近错误：${lastError}。` : "",
    !hasBacklog && lastError ? "请检查 ESP 端 SD 卡挂载、写入权限、剩余空间或卡槽连接。" : "",
  ].filter(Boolean).join("");
  els.alertUploadBtn.hidden = !(hasBacklog && isAdmin());
  els.alertLaterBtn.textContent = hasBacklog ? "稍后提醒" : "我知道了";
  els.uploadAlert.hidden = false;
}

function hideUploadAlert() {
  els.uploadAlert.hidden = true;
  els.alertUploadBtn.hidden = !isAdmin();
  els.alertLaterBtn.textContent = "稍后提醒";
}

function maybeShowUploadAlert(status) {
  const pendingCount = Number(status.pending_count || 0);
  const failedCount = Number(status.failed_count || 0);
  const lastError = String(status.last_error || "").trim();
  if (pendingCount > 0 || failedCount > 0 || lastError) {
    showUploadAlert(status);
  }
}

function renderTable(samples) {
  els.rowCount.textContent = `共 ${sampleTotalCount} 条`;
  updateSamplePaginationUi();

  if (samples.length === 0) {
    els.sampleRows.innerHTML = '<tr><td colspan="6" class="empty">暂无数据</td></tr>';
    return;
  }

  els.sampleRows.innerHTML = samples.map((sample) => `
    <tr>
      <td>${sample.date || ""}</td>
      <td>${sample.time || ""}</td>
      <td>${formatNumber(sample.ph, 2)}</td>
      <td>${formatNumber(sample.temperature, 1)}</td>
      <td>${sample.source || ""}</td>
      <td>${sample.created_at || ""}</td>
    </tr>
  `).join("");
}

function updateSamplePaginationUi() {
  const prevDisabled = currentSamplePage <= 1;
  const nextDisabled = currentSamplePage >= sampleTotalPages;
  els.pageInfo.textContent = `第 ${currentSamplePage} / ${sampleTotalPages} 页`;
  els.chartHint.textContent = `第 ${currentSamplePage} 页 / 每页 ${els.sampleLimit.value || DEFAULT_SAMPLE_LIMIT} 条`;
  els.prevPageBtn.disabled = prevDisabled;
  els.nextPageBtn.disabled = nextDisabled;
  els.chartPrevPageBtn.disabled = prevDisabled;
  els.chartNextPageBtn.disabled = nextDisabled;
}

function goToPreviousSamplePage() {
  if (currentSamplePage > 1) {
    currentSamplePage -= 1;
    loadSamples();
  }
}

function goToNextSamplePage() {
  if (currentSamplePage < sampleTotalPages) {
    currentSamplePage += 1;
    loadSamples();
  }
}

function getSeries(samples, key) {
  return samples
    .slice()
    .reverse()
    .map((sample, index) => ({
      x: index,
      label: `${sample.date} ${sample.time}`,
      value: Number(sample[key]),
      sample,
      sampleIndex: index,
    }))
    .filter((point) => Number.isFinite(point.value));
}

function drawLine(ctx, points, color, metricLabel, xFor, yFor) {
  if (points.length === 0) {
    return;
  }

  ctx.beginPath();
  points.forEach((point, index) => {
    const x = xFor(point.x);
    const y = yFor(point.value);
    if (index === 0) {
      ctx.moveTo(x, y);
    } else {
      ctx.lineTo(x, y);
    }
  });
  ctx.strokeStyle = color;
  ctx.lineWidth = 2;
  ctx.stroke();

  ctx.fillStyle = color;
  points.forEach((point) => {
    const x = xFor(point.x);
    const y = yFor(point.value);
    ctx.beginPath();
    ctx.arc(x, y, 3, 0, Math.PI * 2);
    ctx.fill();
    chartHitPoints.push({
      x,
      y,
      color,
      label: point.label,
      metric: metricLabel,
      value: point.value,
      sample: point.sample,
      sampleIndex: point.sampleIndex,
    });
  });
}

function drawValueLabel(ctx, text, x, y, color, width, height, preferredSide) {
  const paddingX = 7;
  const boxHeight = 24;
  ctx.font = "12px Microsoft YaHei, Arial";
  const boxWidth = Math.ceil(ctx.measureText(text).width + paddingX * 2);
  let boxX = preferredSide === "left" ? x - boxWidth - 10 : x + 10;
  let boxY = y - boxHeight / 2;

  if (boxX + boxWidth > width - 8) {
    boxX = x - boxWidth - 10;
  }
  if (boxX < 8) {
    boxX = x + 10;
  }
  if (boxY < 8) {
    boxY = 8;
  }
  if (boxY + boxHeight > height - 8) {
    boxY = height - boxHeight - 8;
  }

  ctx.fillStyle = "#ffffff";
  ctx.strokeStyle = color;
  ctx.lineWidth = 1.5;
  if (typeof ctx.roundRect === "function") {
    ctx.beginPath();
    ctx.roundRect(boxX, boxY, boxWidth, boxHeight, 5);
    ctx.fill();
    ctx.stroke();
  } else {
    ctx.fillRect(boxX, boxY, boxWidth, boxHeight);
    ctx.strokeRect(boxX, boxY, boxWidth, boxHeight);
  }

  ctx.fillStyle = color;
  ctx.textAlign = "center";
  ctx.textBaseline = "middle";
  ctx.fillText(text, boxX + boxWidth / 2, boxY + boxHeight / 2);
}

function drawSelectedChartRecord(ctx, width, height, pad) {
  if (selectedChartSampleIndex == null) {
    return;
  }

  const points = chartHitPoints.filter((point) => point.sampleIndex === selectedChartSampleIndex);
  if (points.length === 0) {
    return;
  }

  const x = points[0].x;
  const sample = points[0].sample || {};
  ctx.save();
  ctx.strokeStyle = "#475467";
  ctx.lineWidth = 1;
  ctx.setLineDash([4, 4]);
  ctx.beginPath();
  ctx.moveTo(x, pad.top);
  ctx.lineTo(x, height - pad.bottom);
  ctx.stroke();
  ctx.setLineDash([]);

  ctx.fillStyle = "#344054";
  ctx.font = "12px Microsoft YaHei, Arial";
  ctx.textAlign = "center";
  ctx.textBaseline = "top";
  ctx.fillText(`${sample.date || ""} ${sample.time || ""}`.trim(), x, pad.top + 4);

  for (const point of points) {
    ctx.fillStyle = "#ffffff";
    ctx.strokeStyle = point.color;
    ctx.lineWidth = 2;
    ctx.beginPath();
    ctx.arc(point.x, point.y, 5, 0, Math.PI * 2);
    ctx.fill();
    ctx.stroke();

    const valueText = point.metric === "pH"
      ? `pH ${formatNumber(sample.ph, 2)}`
      : `温度 ${formatNumber(sample.temperature, 1)} ℃`;
    drawValueLabel(
      ctx,
      valueText,
      point.x,
      point.y,
      point.color,
      width,
      height,
      point.metric === "pH" ? "right" : "left"
    );
  }

  ctx.restore();
}

function renderChart(samples) {
  if (activePage !== "dashboard") {
    return;
  }

  const canvas = els.chart;
  const ctx = canvas.getContext("2d");
  const rect = canvas.getBoundingClientRect();
  const ratio = window.devicePixelRatio || 1;
  canvas.width = Math.max(640, Math.floor(rect.width * ratio));
  canvas.height = Math.max(280, Math.floor(rect.height * ratio));
  ctx.setTransform(ratio, 0, 0, ratio, 0, 0);

  const width = canvas.width / ratio;
  const height = canvas.height / ratio;
  const pad = { left: 48, right: 18, top: 24, bottom: 42 };
  chartHitPoints = [];

  ctx.clearRect(0, 0, width, height);
  ctx.fillStyle = "#ffffff";
  ctx.fillRect(0, 0, width, height);

  const phPoints = getSeries(samples, "ph");
  const tempPoints = getSeries(samples, "temperature");
  const values = phPoints.concat(tempPoints).map((point) => point.value);

  if (values.length === 0) {
    ctx.fillStyle = "#667085";
    ctx.textAlign = "center";
    ctx.font = "14px Microsoft YaHei, Arial";
    ctx.fillText("暂无趋势数据", width / 2, height / 2);
    return;
  }

  const minValue = Math.min(...values);
  const maxValue = Math.max(...values);
  const span = Math.max(maxValue - minValue, 1);
  const yMin = Math.floor((minValue - span * 0.12) * 10) / 10;
  const yMax = Math.ceil((maxValue + span * 0.12) * 10) / 10;
  const maxIndex = Math.max(samples.length - 1, 1);
  const plotWidth = width - pad.left - pad.right;
  const plotHeight = height - pad.top - pad.bottom;
  const xFor = (index) => pad.left + (index / maxIndex) * plotWidth;
  const yFor = (value) => pad.top + ((yMax - value) / (yMax - yMin)) * plotHeight;

  ctx.strokeStyle = "#d9e0e7";
  ctx.lineWidth = 1;
  ctx.fillStyle = "#667085";
  ctx.font = "12px Microsoft YaHei, Arial";
  ctx.textAlign = "right";
  ctx.textBaseline = "middle";

  for (let i = 0; i <= 4; i += 1) {
    const y = pad.top + (plotHeight / 4) * i;
    const value = yMax - ((yMax - yMin) / 4) * i;
    ctx.beginPath();
    ctx.moveTo(pad.left, y);
    ctx.lineTo(width - pad.right, y);
    ctx.stroke();
    ctx.fillText(value.toFixed(1), pad.left - 8, y);
  }

  drawLine(ctx, phPoints, "#1769aa", "pH", xFor, yFor);
  drawLine(ctx, tempPoints, "#d9822b", "温度", xFor, yFor);

  ctx.textAlign = "left";
  ctx.textBaseline = "alphabetic";
  ctx.fillStyle = "#1769aa";
  ctx.fillText("pH", pad.left, 18);
  ctx.fillStyle = "#d9822b";
  ctx.fillText("温度", pad.left + 42, 18);

  const first = samples[samples.length - 1];
  const last = samples[0];
  ctx.fillStyle = "#667085";
  ctx.textAlign = "left";
  ctx.fillText(first ? `${first.date} ${first.time}` : "", pad.left, height - 14);
  ctx.textAlign = "right";
  ctx.fillText(last ? `${last.date} ${last.time}` : "", width - pad.right, height - 14);

  drawSelectedChartRecord(ctx, width, height, pad);
}

function nearestChartPoint(x, y) {
  let bestPoint = null;
  let bestDistance = Infinity;
  const hitRadius = isChartFullscreen ? 24 : 18;

  for (const point of chartHitPoints) {
    const distance = Math.hypot(point.x - x, point.y - y);
    if (distance < bestDistance) {
      bestDistance = distance;
      bestPoint = point;
    }
  }

  return bestDistance <= hitRadius ? bestPoint : null;
}

function handleChartPointer(event) {
  const touch = event.touches ? event.touches[0] : null;
  const pointer = touch || event;
  const rect = els.chart.getBoundingClientRect();
  const x = pointer.clientX - rect.left;
  const y = pointer.clientY - rect.top;
  const point = nearestChartPoint(x, y);

  if (touch) {
    event.preventDefault();
  }

  if (point) {
    selectedChartSampleIndex = point.sampleIndex;
    renderChart(currentSamples);
  } else {
    selectedChartSampleIndex = null;
    renderChart(currentSamples);
  }
}

function setChartFullscreen(enabled) {
  const panel = document.querySelector(".chart-panel");
  isChartFullscreen = enabled;
  panel.classList.toggle("chart-fullscreen", enabled);
  els.fullscreenChartBtn.textContent = enabled ? "退出全屏" : "全屏查看";
  requestAnimationFrame(() => renderChart(currentSamples));
}

async function toggleChartFullscreen() {
  const panel = document.querySelector(".chart-panel");

  if (!isChartFullscreen) {
    setChartFullscreen(true);
    if (panel.requestFullscreen) {
      try {
        await panel.requestFullscreen();
      } catch {
        // CSS fullscreen fallback remains active.
      }
    }
    return;
  }

  setChartFullscreen(false);
  if (document.fullscreenElement && document.exitFullscreen) {
    try {
      await document.exitFullscreen();
    } catch {
      // Ignore browser-specific fullscreen exit failures.
    }
  }
}

function setActivePage(page) {
  const allowedPages = isAdmin()
    ? ["dashboard", "data", "profile", "commands", "users"]
    : ["dashboard", "data", "profile"];
  activePage = allowedPages.includes(page) ? page : "dashboard";

  els.navTabs.forEach((tab) => {
    tab.classList.toggle("active", tab.dataset.pageTarget === activePage);
  });

  els.pageSections.forEach((section) => {
    section.classList.toggle("is-active", section.dataset.pageSection === activePage);
  });

  window.location.hash = activePage === "dashboard" ? "" : activePage;

  if (activePage === "dashboard") {
    requestAnimationFrame(() => renderChart(currentSamples));
  }

  if (activePage === "users" && isAdmin()) {
    loadUsers();
  }
}

function updateAutoRefreshText() {
  if (!els.autoRefreshText) {
    return;
  }

  const seconds = Math.round(AUTO_REFRESH_MS / 1000);
  const now = new Date();
  const time = `${String(now.getHours()).padStart(2, "0")}:${String(now.getMinutes()).padStart(2, "0")}:${String(now.getSeconds()).padStart(2, "0")}`;
  els.autoRefreshText.textContent = `自动刷新 ${seconds} 秒，上次 ${time}`;
}

function restartAutoRefresh() {
  if (refreshTimer) {
    clearInterval(refreshTimer);
  }

  refreshTimer = setInterval(() => {
    if (!document.hidden) {
      loadSamples({ silent: true });
      loadHealthStatus();
      loadUploadStatus({ silent: true });
      if (isAdmin()) {
        loadCommandHistory();
      }
    }
  }, AUTO_REFRESH_MS);
}

function setCommandMessage(text, state = "") {
  els.commandResult.classList.remove("active", "done", "failed");
  if (state) {
    els.commandResult.classList.add(state);
  }
  els.commandResult.textContent = text;
}

function stopCommandPolling() {
  if (commandTimer) {
    clearInterval(commandTimer);
    commandTimer = null;
  }
}

function formatCommandStatus(command) {
  const statusText = {
    pending: "等待设备获取",
    running: "设备执行中",
    done: "执行完成",
    failed: "执行失败",
    cancelled: "已取消",
  }[command.status] || command.status;

  const result = command.result ? `，结果：${command.result}` : "";
  return `命令 #${command.id}：${statusText}${result}`;
}

async function refreshCommandStatus(commandId) {
  const response = await apiFetch(`/api/commands/${commandId}`);
  if (!response.ok) {
    throw new Error(`HTTP ${response.status}`);
  }

  const data = await response.json();
  const command = data.command;
  const finalStatus = ["done", "failed", "cancelled"].includes(command.status);

  setCommandMessage(
    formatCommandStatus(command),
    finalStatus ? (command.status === "done" ? "done" : "failed") : "active"
  );

  if (finalStatus) {
    stopCommandPolling();
    loadSamples({ silent: true });
    loadUploadStatus({ silent: true });
    if (isAdmin()) {
      loadCommandHistory();
    }
  }
}

function startCommandPolling(commandId) {
  stopCommandPolling();
  refreshCommandStatus(commandId).catch((error) => {
    setCommandMessage(`命令 #${commandId} 状态查询失败：${error.message}`, "failed");
  });

  commandTimer = setInterval(() => {
    refreshCommandStatus(commandId).catch((error) => {
      stopCommandPolling();
      setCommandMessage(`命令 #${commandId} 状态查询失败：${error.message}`, "failed");
    });
  }, COMMAND_REFRESH_MS);
}

async function loadSamples(options = {}) {
  if (isLoading) {
    return;
  }

  isLoading = true;
  if (!options.silent) {
    setStatus(false, "连接中");
  }

  try {
    const response = await apiFetch(buildSampleUrl());
    if (!response.ok) {
      throw new Error(`HTTP ${response.status}`);
    }
    const data = await response.json();
    currentSamples = data.samples || [];
    sampleTotalCount = Number(data.total || currentSamples.length);
    sampleTotalPages = Math.max(Number(data.total_pages || 1), 1);
    currentSamplePage = Math.min(Number(data.page || currentSamplePage), sampleTotalPages);
    setStatus(true, "已连接");
    renderOverview(currentSamples);
    renderTable(currentSamples);
    renderChart(currentSamples);
    updateAutoRefreshText();
    updateSamplePaginationUi();
  } catch (error) {
    setStatus(false, "连接失败");
    els.sampleRows.innerHTML = `<tr><td colspan="6" class="empty">${error.message}</td></tr>`;
    sampleTotalCount = 0;
    sampleTotalPages = 1;
    currentSamplePage = 1;
    updateSamplePaginationUi();
    renderOverview([]);
    renderChart([]);
  } finally {
    isLoading = false;
  }
}

async function loadHealthStatus() {
  try {
    const response = await apiFetch("/health");
    if (!response.ok) {
      throw new Error(`HTTP ${response.status}`);
    }

    const data = await response.json();
    const mqtt = data.mqtt || {};
    if (!mqtt.enabled) {
      setMqttStatus("warning", "MQTT 未启用", "Broker 未配置");
    } else if (mqtt.connected) {
      setMqttStatus("online", "MQTT 已连接", mqtt.url || mqtt.base_topic || "");
    } else {
      setMqttStatus("offline", "MQTT 未连接", mqtt.url || mqtt.base_topic || "");
    }
  } catch (error) {
    setMqttStatus("offline", "状态获取失败", error.message);
  }
}

async function loadUploadStatus(options = {}) {
  try {
    const response = await apiFetch(buildUploadStatusUrl());
    if (!response.ok) {
      throw new Error(`HTTP ${response.status}`);
    }

    const data = await response.json();
    const status = data.status || {};
    renderUploadStatus(status);
    maybeShowUploadAlert(status);
  } catch (error) {
    setUploadStatusIndicator("offline", "未上传状态获取失败");
    if (!options.silent) {
      els.pendingUploadCount.textContent = "--";
      els.failedUploadCount.textContent = "--";
      els.oldestPendingTime.textContent = "--";
      els.lastUploadError.textContent = error.message;
    }
  }
}

function commandStatusLabel(status) {
  return {
    pending: "等待",
    running: "执行中",
    done: "完成",
    failed: "失败",
    cancelled: "取消",
  }[status] || status || "--";
}

function renderCommandHistory(commands) {
  els.commandCount.textContent = `${commands.length} 条`;

  if (commands.length === 0) {
    els.commandRows.innerHTML = '<tr><td colspan="6" class="empty">暂无命令</td></tr>';
    return;
  }

  els.commandRows.innerHTML = commands.map((command) => `
    <tr>
      <td>${command.id}</td>
      <td>${escapeHtml(command.command_name)}</td>
      <td><span class="status-badge ${escapeHtml(command.status)}">${commandStatusLabel(command.status)}</span></td>
      <td class="result-cell" title="${escapeHtml(command.result || "")}">${escapeHtml(command.result || "--")}</td>
      <td>${escapeHtml(command.created_at || "")}</td>
      <td>${escapeHtml(command.finished_at || "--")}</td>
    </tr>
  `).join("");
}

function roleLabel(role) {
  return role === "admin" ? "管理员" : "普通用户";
}

function enabledLabel(enabled) {
  return enabled ? "启用" : "禁用";
}

function setUserFormMessage(text, state = "") {
  els.userFormMessage.classList.remove("active", "done", "failed");
  if (state) {
    els.userFormMessage.classList.add(state);
  }
  els.userFormMessage.textContent = text;
}

function setProfileMessage(text, state = "") {
  els.profileMessage.classList.remove("active", "done", "failed");
  if (state) {
    els.profileMessage.classList.add(state);
  }
  els.profileMessage.textContent = text;
}

async function saveProfile(event) {
  event.preventDefault();
  const form = els.profileForm;
  setProfileMessage("正在保存个人资料...", "active");

  try {
    const body = {
      username: form.username.value.trim(),
      display_name: form.display_name.value.trim(),
    };

    if (form.password.value) {
      body.current_password = form.current_password.value;
      body.password = form.password.value;
    }

    const response = await apiFetch("/api/auth/me", {
      method: "PATCH",
      headers: {
        "Content-Type": "application/json",
      },
      body: JSON.stringify(body),
    });
    const data = await response.json().catch(() => ({}));
    if (!response.ok) {
      throw new Error(data.message || `HTTP ${response.status}`);
    }

    form.current_password.value = "";
    form.password.value = "";
    applyUser(data.user);
    setProfileMessage("个人资料已更新", "done");
    if (isAdmin()) {
      loadUsers();
    }
  } catch (error) {
    setProfileMessage(error.message, "failed");
  }
}

function renderUsers(users) {
  els.userCount.textContent = `${users.length} 个`;

  if (users.length === 0) {
    els.userRows.innerHTML = '<tr><td colspan="7" class="empty">暂无用户</td></tr>';
    return;
  }

  els.userRows.innerHTML = users.map((user) => {
    const isSelf = currentUser && currentUser.id === user.id;
    const nextRole = user.role === "admin" ? "user" : "admin";
    const nextEnabled = !user.enabled;
    return `
      <tr>
        <td>${user.id}</td>
        <td>${escapeHtml(user.username)}${isSelf ? "（当前）" : ""}</td>
        <td>${escapeHtml(user.display_name || "")}</td>
        <td><span class="status-badge ${escapeHtml(user.role)}">${roleLabel(user.role)}</span></td>
        <td><span class="status-badge ${user.enabled ? "done" : "failed"}">${enabledLabel(user.enabled)}</span></td>
        <td>${escapeHtml(user.last_login_at || "--")}</td>
        <td class="user-actions">
          <button class="secondary" type="button" data-user-action="role" data-user-id="${user.id}" data-value="${nextRole}" ${isSelf ? "disabled" : ""}>设为${roleLabel(nextRole)}</button>
          <button class="secondary" type="button" data-user-action="enabled" data-user-id="${user.id}" data-value="${nextEnabled}" ${isSelf ? "disabled" : ""}>${nextEnabled ? "启用" : "禁用"}</button>
          <button class="secondary" type="button" data-user-action="password" data-user-id="${user.id}">重置密码</button>
          <button class="danger" type="button" data-user-action="delete" data-user-id="${user.id}" data-username="${escapeHtml(user.username)}" ${isSelf ? "disabled" : ""}>删除</button>
        </td>
      </tr>
    `;
  }).join("");
}

async function loadUsers() {
  if (!isAdmin()) {
    return;
  }

  try {
    const response = await apiFetch("/api/users");
    if (!response.ok) {
      throw new Error(`HTTP ${response.status}`);
    }
    const data = await response.json();
    renderUsers(data.users || []);
  } catch (error) {
    els.userRows.innerHTML = `<tr><td colspan="7" class="empty">${escapeHtml(error.message)}</td></tr>`;
  }
}

async function createUser(event) {
  event.preventDefault();
  if (!isAdmin()) {
    return;
  }

  const form = els.userForm;
  setUserFormMessage("正在创建用户...", "active");

  try {
    const response = await apiFetch("/api/users", {
      method: "POST",
      headers: {
        "Content-Type": "application/json",
      },
      body: JSON.stringify({
        username: form.username.value.trim(),
        display_name: form.display_name.value.trim(),
        password: form.password.value,
        role: form.role.value,
      }),
    });
    const data = await response.json().catch(() => ({}));
    if (!response.ok) {
      throw new Error(data.message || `HTTP ${response.status}`);
    }

    form.reset();
    setUserFormMessage(`已创建用户 ${data.user.username}`, "done");
    loadUsers();
  } catch (error) {
    setUserFormMessage(error.message, "failed");
  }
}

async function patchUser(userId, patch) {
  const response = await apiFetch(`/api/users/${userId}`, {
    method: "PATCH",
    headers: {
      "Content-Type": "application/json",
    },
    body: JSON.stringify(patch),
  });
  const data = await response.json().catch(() => ({}));
  if (!response.ok) {
    throw new Error(data.message || `HTTP ${response.status}`);
  }
  return data.user;
}

async function deleteUser(userId) {
  const response = await apiFetch(`/api/users/${userId}`, {
    method: "DELETE",
  });
  const data = await response.json().catch(() => ({}));
  if (!response.ok) {
    throw new Error(data.message || `HTTP ${response.status}`);
  }
  return data.user;
}

async function handleUserAction(event) {
  const button = event.target.closest("[data-user-action]");
  if (!button || !isAdmin()) {
    return;
  }

  const userId = button.dataset.userId;
  const action = button.dataset.userAction;
  const patch = {};

  if (action === "role") {
    patch.role = button.dataset.value;
  } else if (action === "enabled") {
    patch.enabled = button.dataset.value === "true";
  } else if (action === "password") {
    const password = window.prompt("请输入新密码，长度 6-64 位");
    if (!password) {
      return;
    }
    patch.password = password;
  } else if (action === "delete") {
    const username = button.dataset.username;
    const confirmed = window.prompt(`删除用户不可恢复。请输入用户名 ${username} 确认删除`);
    if (confirmed !== username) {
      setUserFormMessage("用户名确认不匹配，已取消删除", "failed");
      return;
    }
  }

  button.disabled = true;
  setUserFormMessage(action === "delete" ? "正在删除用户..." : "正在更新用户...", "active");
  try {
    if (action === "delete") {
      const user = await deleteUser(userId);
      setUserFormMessage(`已删除用户 ${user.username}`, "done");
    } else {
      await patchUser(userId, patch);
      setUserFormMessage("用户已更新", "done");
    }
    loadUsers();
  } catch (error) {
    setUserFormMessage(error.message, "failed");
    button.disabled = false;
  }
}

async function loadCommandHistory() {
  if (!isAdmin()) {
    return;
  }

  try {
    const params = new URLSearchParams({
      device_id: DEVICE_ID,
      limit: "10",
    });
    const response = await apiFetch(`/api/commands?${params.toString()}`);
    if (!response.ok) {
      throw new Error(`HTTP ${response.status}`);
    }

    const data = await response.json();
    renderCommandHistory(data.commands || []);
  } catch (error) {
    els.commandRows.innerHTML = `<tr><td colspan="6" class="empty">${escapeHtml(error.message)}</td></tr>`;
  }
}

function currentMonth() {
  const now = new Date();
  const month = String(now.getMonth() + 1).padStart(2, "0");
  return `${now.getFullYear()}-${month}`;
}

async function createUploadCommand() {
  if (!isAdmin()) {
    setCommandMessage("普通用户只有查看权限", "failed");
    return;
  }

  const targetMonth = els.commandMonth.value || currentMonth();
  if (!/^\d{4}-\d{2}$/.test(targetMonth)) {
    setCommandMessage("请选择要补传的月份", "failed");
    return;
  }

  els.uploadPendingBtn.disabled = true;
  els.alertUploadBtn.disabled = true;
  stopCommandPolling();
  setCommandMessage(`正在创建 ${targetMonth} 的补传命令`, "active");

  try {
    const response = await apiFetch("/api/commands", {
      method: "POST",
      headers: {
        "Content-Type": "application/json",
      },
      body: JSON.stringify({
        device_id: DEVICE_ID,
        command_name: "UPLOAD_PENDING",
        payload: {
          month: targetMonth,
          force: true,
        },
      }),
    });

    if (!response.ok) {
      throw new Error(`HTTP ${response.status}`);
    }

    const data = await response.json();
    hideUploadAlert();
    setCommandMessage(`已创建命令 #${data.command.id}，已下发 ${targetMonth} 的补传任务`, "active");
    startCommandPolling(data.command.id);
    loadCommandHistory();
  } catch (error) {
    setCommandMessage(`发送失败：${error.message}`, "failed");
  } finally {
    els.uploadPendingBtn.disabled = false;
    els.alertUploadBtn.disabled = false;
  }
}

els.queryBtn.addEventListener("click", () => {
  currentSamplePage = 1;
  loadSamples();
});
els.refreshBtn.addEventListener("click", loadSamples);
els.resetBtn.addEventListener("click", () => {
  els.singleDate.value = "";
  els.startDate.value = "";
  els.endDate.value = "";
  els.sampleLimit.value = String(DEFAULT_SAMPLE_LIMIT);
  currentSamplePage = 1;
  loadSamples();
});
els.singleDate.addEventListener("change", () => {
  if (els.singleDate.value) {
    els.startDate.value = "";
    els.endDate.value = "";
  }
});
els.startDate.addEventListener("change", () => {
  if (els.startDate.value) {
    els.singleDate.value = "";
  }
});
els.endDate.addEventListener("change", () => {
  if (els.endDate.value) {
    els.singleDate.value = "";
  }
});
els.commandMonth.addEventListener("change", () => {
  commandMonthTouched = true;
});
els.sampleLimit.addEventListener("change", () => {
  currentSamplePage = 1;
  loadSamples();
});
els.prevPageBtn.addEventListener("click", goToPreviousSamplePage);
els.nextPageBtn.addEventListener("click", goToNextSamplePage);
els.chartPrevPageBtn.addEventListener("click", goToPreviousSamplePage);
els.chartNextPageBtn.addEventListener("click", goToNextSamplePage);
els.uploadPendingBtn.addEventListener("click", createUploadCommand);
els.alertUploadBtn.addEventListener("click", createUploadCommand);
els.profileForm.addEventListener("submit", saveProfile);
els.userForm.addEventListener("submit", createUser);
els.userRows.addEventListener("click", handleUserAction);
els.alertLaterBtn.addEventListener("click", () => {
  uploadAlertDismissedKey = lastUploadAlertKey;
  hideUploadAlert();
});
els.logoutBtn.addEventListener("click", async () => {
  await fetch("/api/auth/logout", { method: "POST" }).catch(() => {});
  window.location.replace("/login.html");
});
els.fullscreenChartBtn.addEventListener("click", toggleChartFullscreen);
els.chart.addEventListener("click", handleChartPointer);
els.chart.addEventListener("touchstart", handleChartPointer, { passive: false });
els.navTabs.forEach((tab) => {
  tab.addEventListener("click", () => setActivePage(tab.dataset.pageTarget));
});
window.addEventListener("resize", () => renderChart(currentSamples));
document.addEventListener("fullscreenchange", () => {
  const panel = document.querySelector(".chart-panel");
  if (!document.fullscreenElement && panel.classList.contains("chart-fullscreen")) {
    setChartFullscreen(false);
  }
});
document.addEventListener("visibilitychange", () => {
  if (!document.hidden) {
    loadSamples({ silent: true });
    loadHealthStatus();
    loadUploadStatus({ silent: true });
    if (isAdmin()) {
      loadCommandHistory();
      loadUsers();
    }
  }
});

async function initializeApp() {
  try {
    const response = await apiFetch("/api/auth/me");
    const data = await response.json();
    applyUser(data.user);
    els.commandMonth.value = currentMonth();
    setActivePage(window.location.hash.replace("#", "") || "dashboard");
    loadHealthStatus();
    loadUploadStatus();
    loadSamples();
    if (isAdmin()) {
      loadCommandHistory();
    }
    restartAutoRefresh();
  } catch (error) {
    setStatus(false, error.message);
  }
}

initializeApp();
