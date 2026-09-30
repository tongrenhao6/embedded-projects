/**
 * 蜂箱环境监测系统 - 前端交互核心脚本
 * 功能：多网关/节点切换、实时指标刷新、2秒自动轮询、姿态倾倒告警、Canvas 历史双曲线绘制
 */

(function () {
  'use strict';

  // 全局应用状态
  const state = {
    gateways: [],
    selectedGatewayId: '',
    gatewayStatus: null,
    nodes: [],
    selectedNodeId: null,
    latestReading: null,
    history: [],
    historyLimit: 20,
    autoRefresh: true,
    refreshIntervalSeconds: 2,
    countdown: 2,
    timerId: null,
    isFetching: false,
    hoveredTempIndex: -1,
    hoveredHumIndex: -1
  };

  // DOM 元素引用
  const el = {
    systemStatus: document.getElementById('systemStatus'),
    autoRefreshToggle: document.getElementById('autoRefreshToggle'),
    refreshTimerLabel: document.getElementById('refreshTimerLabel'),
    refreshBtn: document.getElementById('refreshBtn'),
    alarmBanner: document.getElementById('alarmBanner'),
    alarmTitle: document.getElementById('alarmTitle'),
    alarmDesc: document.getElementById('alarmDesc'),
    alarmBadge: document.getElementById('alarmBadge'),
    errorBanner: document.getElementById('errorBanner'),
    errorMessage: document.getElementById('errorMessage'),
    dismissErrorBtn: document.getElementById('dismissErrorBtn'),
    gatewaySelect: document.getElementById('gatewaySelect'),
    gatewayStatusBadge: document.getElementById('gatewayStatusBadge'),
    nodeSelect: document.getElementById('nodeSelect'),
    nodeCountBadge: document.getElementById('nodeCountBadge'),
    nodeStatusBadge: document.getElementById('nodeStatusBadge'),
    lastUpdateTime: document.getElementById('lastUpdateTime'),

    // 核心指标
    valInsideTemp: document.getElementById('valInsideTemp'),
    valInsideHum: document.getElementById('valInsideHum'),
    valOutsideTemp: document.getElementById('valOutsideTemp'),
    valOutsideHum: document.getElementById('valOutsideHum'),

    // 姿态与诊断
    tiltIndicator: document.getElementById('tiltIndicator'),
    tiltIcon: document.getElementById('tiltIcon'),
    tiltText: document.getElementById('tiltText'),
    tiltDesc: document.getElementById('tiltDesc'),
    valStatusFlags: document.getElementById('valStatusFlags'),
    statusFlagTags: document.getElementById('statusFlagTags'),
    valTempDiff: document.getElementById('valTempDiff'),
    valHumDiff: document.getElementById('valHumDiff'),
    valSequenceNo: document.getElementById('valSequenceNo'),
    valUptime: document.getElementById('valUptime'),
    valGatewayStatus: document.getElementById('valGatewayStatus'),
    valNodeStatus: document.getElementById('valNodeStatus'),
    valReceivedAt: document.getElementById('valReceivedAt'),

    // 图表与表格
    tempCanvas: document.getElementById('tempChart'),
    humCanvas: document.getElementById('humChart'),
    tempChartEmpty: document.getElementById('tempChartEmpty'),
    humChartEmpty: document.getElementById('humChartEmpty'),
    recordsCount: document.getElementById('recordsCount'),
    historyTableBody: document.getElementById('historyTableBody'),
    exportCsvLink: document.getElementById('exportCsvLink'),
    qualityLink: document.getElementById('qualityLink'),
    limitButtons: document.querySelectorAll('.btn-chip')
  };

  // =========================================================================
  // 辅助格式化函数
  // =========================================================================

  /**
   * 安全格式化数值，严格区分 null 与 0
   */
  function formatNum(val, decimals = 2) {
    if (val === null || val === undefined || isNaN(Number(val))) {
      return '--';
    }
    return Number(val).toFixed(decimals);
  }

  /**
   * 格式化运行毫秒为可读字符串
   */
  function formatUptime(ms) {
    if (ms === null || ms === undefined || isNaN(Number(ms))) {
      return '--';
    }
    const totalSec = Math.floor(Number(ms) / 1000);
    const days = Math.floor(totalSec / 86400);
    const hours = Math.floor((totalSec % 86400) / 3600);
    const minutes = Math.floor((totalSec % 3600) / 60);
    const seconds = totalSec % 60;

    const parts = [];
    if (days > 0) parts.push(`${days}天`);
    if (hours > 0 || days > 0) parts.push(`${hours}小时`);
    if (minutes > 0 || hours > 0 || days > 0) parts.push(`${minutes}分`);
    parts.push(`${seconds}秒`);
    return `${parts.join('')} (${ms} ms)`;
  }

  /**
   * 格式化时间戳字符串
   */
  function formatTimestamp(isoStr) {
    if (!isoStr) return '--';
    try {
      const d = new Date(isoStr);
      if (isNaN(d.getTime())) return String(isoStr);
      const pad = (n) => String(n).padStart(2, '0');
      return `${d.getFullYear()}-${pad(d.getMonth() + 1)}-${pad(d.getDate())} ${pad(d.getHours())}:${pad(d.getMinutes())}:${pad(d.getSeconds())}`;
    } catch {
      return String(isoStr);
    }
  }

  /**
   * 格式化简短时间 (HH:mm:ss)
   */
  function formatShortTime(isoStr) {
    if (!isoStr) return '--';
    try {
      const d = new Date(isoStr);
      if (isNaN(d.getTime())) return String(isoStr).slice(-8);
      const pad = (n) => String(n).padStart(2, '0');
      return `${pad(d.getHours())}:${pad(d.getMinutes())}:${pad(d.getSeconds())}`;
    } catch {
      return String(isoStr);
    }
  }

  // =========================================================================
  // API 请求包装器
  // =========================================================================

  async function apiFetch(url) {
    const res = await fetch(url);
    if (!res.ok) {
      const text = await res.text();
      let errorMsg = `HTTP ${res.status}`;
      try {
        const json = JSON.parse(text);
        if (json.detail) errorMsg += `: ${json.detail}`;
      } catch {
        if (text) errorMsg += `: ${text}`;
      }
      throw new Error(errorMsg);
    }
    return await res.json();
  }

  function showError(msg) {
    el.errorMessage.textContent = msg;
    el.errorBanner.classList.remove('hidden');
    el.systemStatus.innerHTML = '<span class="status-dot offline"></span><span class="status-text">连接异常</span>';
  }

  function clearError() {
    el.errorBanner.classList.add('hidden');
    el.systemStatus.innerHTML = '<span class="status-dot online"></span><span class="status-text">系统正常</span>';
  }

  // =========================================================================
  // 数据加载与状态同步
  // =========================================================================

  /**
   * 初始加载网关列表
   */
  async function loadGateways(preserveSelection = true) {
    try {
      const list = await apiFetch('/api/gateways');
      state.gateways = Array.isArray(list) ? list : [];

      el.gatewaySelect.innerHTML = '';
      if (state.gateways.length === 0) {
        el.gatewaySelect.innerHTML = '<option value="">暂无可用网关</option>';
        state.selectedGatewayId = '';
        updateGatewayUI();
        await loadNodes();
        return;
      }

      state.gateways.forEach((gw) => {
        const opt = document.createElement('option');
        opt.value = gw.gateway_id;
        const statusText = gw.status === 'online' ? '🟢 在线' : '⚪ 离线';
        opt.textContent = `${gw.gateway_id} (${statusText})`;
        el.gatewaySelect.appendChild(opt);
      });

      // 维持之前选中的网关，或者默认选择第一个
      if (preserveSelection && state.selectedGatewayId && state.gateways.some(g => g.gateway_id === state.selectedGatewayId)) {
        el.gatewaySelect.value = state.selectedGatewayId;
      } else {
        state.selectedGatewayId = state.gateways[0].gateway_id;
        el.gatewaySelect.value = state.selectedGatewayId;
      }

      updateGatewayUI();
      await loadNodes(preserveSelection);
    } catch (err) {
      console.error('加载网关列表失败:', err);
      showError(`加载网关列表失败: ${err.message}`);
    }
  }

  /**
   * 更新网关状态徽章
   */
  function updateGatewayUI() {
    const currentGw = state.gateways.find(g => g.gateway_id === state.selectedGatewayId);
    if (!currentGw) {
      el.gatewayStatusBadge.className = 'badge badge-neutral';
      el.gatewayStatusBadge.textContent = '无网关';
      el.valGatewayStatus.textContent = '暂无网关';
      return;
    }

    const isOnline = currentGw.status === 'online';
    el.gatewayStatusBadge.className = isOnline ? 'badge badge-success' : 'badge badge-danger';
    el.gatewayStatusBadge.textContent = isOnline ? '网关在线' : '网关离线';

    const lastSeen = currentGw.last_seen_at ? formatTimestamp(currentGw.last_seen_at) : '从未上报';
    el.valGatewayStatus.innerHTML = isOnline
      ? `<span style="color:#059669;font-weight:700">在线</span> <small style="color:#64748b">(${lastSeen})</small>`
      : `<span style="color:#dc2626;font-weight:700">离线</span> <small style="color:#64748b">(最后活跃: ${lastSeen})</small>`;
  }

  function nodeStatusText(status) {
    return ({ online: '在线', stale: '通信不稳定', offline: '离线', unknown: '暂无数据' })[status] || '未知';
  }

  function updateNodeUI(nodeOrReading = null) {
    const node = nodeOrReading || state.nodes.find((item) => Number(item.node_id) === Number(state.selectedNodeId));
    const status = node && node.status ? node.status : 'unknown';
    const badgeClass = ({ online: 'badge-success', stale: 'badge-warning', offline: 'badge-danger' })[status] || 'badge-neutral';
    el.nodeStatusBadge.className = `badge ${badgeClass}`;
    el.nodeStatusBadge.textContent = `节点${nodeStatusText(status)}`;

    if (!node || !node.last_seen_at) {
      el.valNodeStatus.textContent = '暂无节点上报';
      return;
    }
    const age = Number(node.last_seen_age_seconds);
    const ageText = Number.isFinite(age) ? `${Math.max(0, Math.round(age))} 秒前` : '时间未知';
    const suffix = status === 'online' ? '' : '，最新测量值已保留但已过期';
    el.valNodeStatus.innerHTML = `<span class="node-state-${status}">${nodeStatusText(status)}</span> <small>(${ageText}，${formatTimestamp(node.last_seen_at)}${suffix})</small>`;
  }

  /**
   * 加载当前选中网关下的节点列表
   */
  async function loadNodes(preserveSelection = true) {
    if (!state.selectedGatewayId) {
      state.nodes = [];
      state.selectedNodeId = null;
      el.nodeSelect.innerHTML = '<option value="">请先选择网关</option>';
      el.nodeSelect.disabled = true;
      el.nodeCountBadge.textContent = '节点: 0';
      updateNodeUI(null);
      resetTelemetryUI();
      return;
    }

    try {
      const nodes = await apiFetch(`/api/gateways/${encodeURIComponent(state.selectedGatewayId)}/nodes`);
      state.nodes = Array.isArray(nodes) ? nodes : [];

      el.nodeSelect.innerHTML = '';
      if (state.nodes.length === 0) {
        el.nodeSelect.innerHTML = '<option value="">该网关暂无节点</option>';
        el.nodeSelect.disabled = true;
        el.nodeCountBadge.textContent = '节点: 0';
        state.selectedNodeId = null;
        updateNodeUI(null);
        resetTelemetryUI();
        return;
      }

      el.nodeSelect.disabled = false;
      el.nodeCountBadge.textContent = `节点: ${state.nodes.length}`;

      state.nodes.forEach((node) => {
        const opt = document.createElement('option');
        opt.value = String(node.node_id);
        opt.textContent = `蜂箱节点 #${node.node_id}（${nodeStatusText(node.status)}）`;
        el.nodeSelect.appendChild(opt);
      });

      // 维持之前选中的节点或默认选择第一个
      const currentSelectedNodeExists = state.nodes.some(n => Number(n.node_id) === Number(state.selectedNodeId));
      if (preserveSelection && state.selectedNodeId !== null && currentSelectedNodeExists) {
        el.nodeSelect.value = String(state.selectedNodeId);
      } else {
        state.selectedNodeId = Number(state.nodes[0].node_id);
        el.nodeSelect.value = String(state.selectedNodeId);
      }

      updateNodeUI();
      await loadNodeTelemetry();
    } catch (err) {
      console.error('加载节点列表失败:', err);
      showError(`加载节点列表失败: ${err.message}`);
    }
  }

  /**
   * 加载当前选定节点的最新数据与历史数据
   */
  async function loadNodeTelemetry() {
    if (!state.selectedGatewayId || state.selectedNodeId === null) {
      resetTelemetryUI();
      return;
    }

    try {
      const gwId = encodeURIComponent(state.selectedGatewayId);
      const nodeId = encodeURIComponent(state.selectedNodeId);

      // 导出与质量统计链接跟随当前选中的网关/节点
      el.exportCsvLink.href = `/api/gateways/${gwId}/nodes/${nodeId}/minute-history/export`;
      el.exportCsvLink.removeAttribute('aria-disabled');
      el.qualityLink.href = `/api/gateways/${gwId}/nodes/${nodeId}/quality?hours=24`;
      el.qualityLink.removeAttribute('aria-disabled');

      // 并行获取最新读数与历史记录
      const [latestRes, historyRes] = await Promise.allSettled([
        apiFetch(`/api/gateways/${gwId}/nodes/${nodeId}/latest`),
        apiFetch(`/api/gateways/${gwId}/nodes/${nodeId}/minute-history?limit=${state.historyLimit}`)
      ]);

      if (latestRes.status === 'fulfilled') {
        state.latestReading = latestRes.value;
        updateNodeUI(latestRes.value);
        renderLatestTelemetry(latestRes.value);
      } else {
        console.warn('获取最新数据失败:', latestRes.reason);
        state.latestReading = null;
        resetTelemetryUI();
        updateNodeUI();
      }

      if (historyRes.status === 'fulfilled') {
        state.history = Array.isArray(historyRes.value) ? historyRes.value : [];
        renderHistoryTable(state.history);
        renderCharts(state.history);
      } else {
        console.warn('获取历史数据失败:', historyRes.reason);
        state.history = [];
        renderHistoryTable([]);
        renderCharts([]);
      }

      el.lastUpdateTime.textContent = new Date().toLocaleTimeString();
      if (latestRes.status === 'fulfilled' && historyRes.status === 'fulfilled') {
        clearError();
      } else {
        showError('部分监测数据暂时无法加载，请稍后重试');
      }
    } catch (err) {
      console.error('刷新遥测数据失败:', err);
      showError(`刷新数据异常: ${err.message}`);
    }
  }

  /**
   * 重置所有遥测展示区域为暂无数据
   */
  function resetTelemetryUI() {
    el.valInsideTemp.textContent = '--';
    el.valInsideHum.textContent = '--';
    el.valOutsideTemp.textContent = '--';
    el.valOutsideHum.textContent = '--';
    el.valTempDiff.textContent = '--';
    el.valHumDiff.textContent = '--';
    el.valSequenceNo.textContent = '--';
    el.valUptime.textContent = '--';
    el.valReceivedAt.textContent = '--';
    el.valStatusFlags.textContent = '--';
    updateNodeUI(null);

    el.alarmBanner.classList.add('hidden');
    el.tiltIndicator.className = 'tilt-status-indicator unknown';
    el.tiltIcon.textContent = '⚪';
    el.tiltText.textContent = '暂无数据';
    el.tiltDesc.textContent = '当前未选中有效节点或暂无上报';
    el.statusFlagTags.innerHTML = '<span class="flag-pill">暂无数据</span>';

    el.exportCsvLink.setAttribute('aria-disabled', 'true');
    el.qualityLink.setAttribute('aria-disabled', 'true');

    renderHistoryTable([]);
    renderCharts([]);
  }

  // =========================================================================
  // 核心渲染逻辑
  // =========================================================================

  /**
   * 渲染最新上报指标
   */
  function renderLatestTelemetry(data) {
    if (!data) {
      resetTelemetryUI();
      return;
    }

    // 1. 温湿度数值与单位
    el.valInsideTemp.textContent = formatNum(data.inside_temperature);
    el.valInsideHum.textContent = formatNum(data.inside_humidity);
    el.valOutsideTemp.textContent = formatNum(data.outside_temperature);
    el.valOutsideHum.textContent = formatNum(data.outside_humidity);

    // 2. 温湿度差异计算
    if (data.inside_temperature !== null && data.outside_temperature !== null) {
      const tDiff = Number(data.inside_temperature) - Number(data.outside_temperature);
      const sign = tDiff > 0 ? '+' : '';
      el.valTempDiff.textContent = `${sign}${tDiff.toFixed(2)} °C`;
      el.valTempDiff.style.color = tDiff >= 0 ? '#ea580c' : '#0284c7';
    } else {
      el.valTempDiff.textContent = '--';
      el.valTempDiff.style.color = 'inherit';
    }

    if (data.inside_humidity !== null && data.outside_humidity !== null) {
      const hDiff = Number(data.inside_humidity) - Number(data.outside_humidity);
      const sign = hDiff > 0 ? '+' : '';
      el.valHumDiff.textContent = `${sign}${hDiff.toFixed(2)} %RH`;
      el.valHumDiff.style.color = hDiff >= 0 ? '#0891b2' : '#6366f1';
    } else {
      el.valHumDiff.textContent = '--';
      el.valHumDiff.style.color = 'inherit';
    }

    // 3. 序列号与运行时间
    el.valSequenceNo.textContent = data.sequence_no !== null && data.sequence_no !== undefined ? `#${data.sequence_no}` : '--';
    el.valUptime.textContent = formatUptime(data.uptime_ms);
    el.valReceivedAt.textContent = formatTimestamp(data.received_at);

    // 4. 后端根据有效位规范化 tilted；null 表示姿态数据无效或缺失。
    const isTilted = data.tilted === true || data.tilted === 1;
    const isUnknownTilt = data.tilted === null || data.tilted === undefined;

    if (isTilted) {
      el.alarmBanner.classList.remove('hidden');
      el.alarmTitle.textContent = `⚠️ 节点 #${data.node_id} 发生倾倒告警！`;
      el.alarmDesc.textContent = `检测到蜂箱姿态异常，当前已发生倾倒！数据上报序列号: #${data.sequence_no}，请立即排查现场情况！`;
      el.alarmBadge.textContent = `status_flags = ${data.status_flags !== undefined ? data.status_flags : '--'}`;

      el.tiltIndicator.className = 'tilt-status-indicator tilted';
      el.tiltIcon.textContent = '🚨';
      el.tiltText.textContent = '⚠️ 已倾倒 (告警中)';
      el.tiltDesc.textContent = '蜂箱姿态发生剧烈倾斜或倾倒，请及时干预！';
    } else if (isUnknownTilt) {
      el.alarmBanner.classList.add('hidden');
      el.tiltIndicator.className = 'tilt-status-indicator unknown';
      el.tiltIcon.textContent = '❓';
      el.tiltText.textContent = '姿态未知 (null)';
      el.tiltDesc.textContent = '传感器未提供倾倒有效位';
    } else {
      el.alarmBanner.classList.add('hidden');
      el.tiltIndicator.className = 'tilt-status-indicator';
      el.tiltIcon.textContent = '🛡️';
      el.tiltText.textContent = '正常 (稳固立正)';
      el.tiltDesc.textContent = '蜂箱放置稳固，未发生异常倾斜';
    }

    // 5. 状态标志位 status_flags 解构
    const flags = Number(data.status_flags);
    el.valStatusFlags.textContent = !isNaN(flags) ? `${flags} (0x${flags.toString(16).toUpperCase().padStart(2, '0')})` : '--';

    if (!isNaN(flags)) {
      const bitInside = (flags & 1) !== 0;
      const bitOutside = (flags & 2) !== 0;
      const bitTiltValid = (flags & 4) !== 0;
      const bitTiltAlarm = (flags & 8) !== 0;

      el.statusFlagTags.innerHTML = `
        <span class="flag-pill ${bitInside ? 'active-ok' : ''}">内部温湿: ${bitInside ? '有效' : '无效'}</span>
        <span class="flag-pill ${bitOutside ? 'active-ok' : ''}">外部温湿: ${bitOutside ? '有效' : '无效'}</span>
        <span class="flag-pill ${bitTiltValid ? 'active-ok' : ''}">姿态检测: ${bitTiltValid ? '有效' : '无效'}</span>
        <span class="flag-pill ${bitTiltAlarm ? 'active-warn' : 'active-ok'}">倾倒告警: ${bitTiltAlarm ? '已触发' : '未触发'}</span>
      `;
    } else {
      el.statusFlagTags.innerHTML = '<span class="flag-pill">未解析标志位</span>';
    }
  }

  /**
   * 渲染历史明细表格
   */
  function renderHistoryTable(records) {
    el.recordsCount.textContent = `共 ${records.length} 条记录`;

    if (!records || records.length === 0) {
      el.historyTableBody.innerHTML = `
        <tr>
          <td colspan="9" class="table-placeholder">暂无历史数据记录</td>
        </tr>
      `;
      return;
    }

    const rowsHtml = records.map((row) => {
      const isAlarm = row.tilted === true || row.tilted === 1;
      const isNormal = row.tilted === false || row.tilted === 0;
      const tiltBadge = isAlarm
        ? '<span class="badge badge-danger">已倾倒</span>'
        : (isNormal ? '<span class="badge badge-success">正常</span>' : '<span class="badge badge-neutral">--</span>');

      return `
        <tr class="${isAlarm ? 'row-alarm' : ''}">
          <td class="font-mono">#${row.sequence_no}</td>
          <td class="font-mono">${formatTimestamp(row.received_at)}</td>
          <td><strong>${formatNum(row.inside_temperature)}</strong> °C</td>
          <td><strong>${formatNum(row.inside_humidity)}</strong> %RH</td>
          <td>${formatNum(row.outside_temperature)} °C</td>
          <td>${formatNum(row.outside_humidity)} %RH</td>
          <td>${tiltBadge}</td>
          <td class="font-mono">${row.status_flags !== undefined ? row.status_flags : '--'}</td>
          <td class="font-mono">${formatUptime(row.uptime_ms)}</td>
        </tr>
      `;
    }).join('');

    el.historyTableBody.innerHTML = rowsHtml;
  }

  // =========================================================================
  // Canvas 历史趋势双图表渲染引擎 (原生、高分辨率、平滑交互)
  // =========================================================================

  function renderCharts(records) {
    if (!records || records.length === 0) {
      el.tempChartEmpty.classList.remove('hidden');
      el.humChartEmpty.classList.remove('hidden');
      clearCanvas(el.tempCanvas);
      clearCanvas(el.humCanvas);
      return;
    }

    // 历史接口返回的按时间降序 (最新在前)，图表从左往右需要升序 (早到晚)
    const chartData = [...records].reverse();
    const hasValues = (keys) => chartData.some((record) => keys.some((key) => {
      const value = record[key];
      return value !== null && value !== undefined && Number.isFinite(Number(value));
    }));

    const hasTemperature = hasValues(['inside_temperature', 'outside_temperature']);
    const hasHumidity = hasValues(['inside_humidity', 'outside_humidity']);

    el.tempChartEmpty.classList.toggle('hidden', hasTemperature);
    el.humChartEmpty.classList.toggle('hidden', hasHumidity);

    if (hasTemperature) {
      drawDualLineChart({
        canvas: el.tempCanvas,
        data: chartData,
        series1: { key: 'inside_temperature', name: '箱内温度', color: '#ea580c', unit: '°C' },
        series2: { key: 'outside_temperature', name: '箱外温度', color: '#0284c7', unit: '°C' },
        hoverIndex: state.hoveredTempIndex,
        onHover: (idx) => {
          state.hoveredTempIndex = idx;
          renderCharts(state.history);
        }
      });
    } else {
      clearCanvas(el.tempCanvas);
    }

    if (hasHumidity) {
      drawDualLineChart({
        canvas: el.humCanvas,
        data: chartData,
        series1: { key: 'inside_humidity', name: '箱内湿度', color: '#0891b2', unit: '%RH' },
        series2: { key: 'outside_humidity', name: '箱外湿度', color: '#6366f1', unit: '%RH' },
        hoverIndex: state.hoveredHumIndex,
        onHover: (idx) => {
          state.hoveredHumIndex = idx;
          renderCharts(state.history);
        }
      });
    } else {
      clearCanvas(el.humCanvas);
    }
  }

  function clearCanvas(canvas) {
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    ctx.clearRect(0, 0, canvas.width, canvas.height);
  }

  /**
   * 通用双折线图绘制函数
   */
  function drawDualLineChart(cfg) {
    const { canvas, data, series1, series2, hoverIndex, onHover } = cfg;
    if (!canvas || !data || data.length === 0) return;

    const rect = canvas.getBoundingClientRect();
    const dpr = window.devicePixelRatio || 1;
    const width = rect.width || 400;
    const height = rect.height || 260;

    canvas.width = width * dpr;
    canvas.height = height * dpr;

    const ctx = canvas.getContext('2d');
    ctx.scale(dpr, dpr);

    // 边距
    const pad = { top: 25, right: 25, bottom: 35, left: 45 };
    const chartW = width - pad.left - pad.right;
    const chartH = height - pad.top - pad.bottom;

    // 每次绘制刷新悬停所需的几何与数据；事件处理器统一从这里读取，
    // 避免闭包捕获首次绘制的 data/pad，切换历史条数或节点后悬浮定位偏左。
    canvas._chartState = { data, width, padLeft: pad.left, chartW, onHover };

    if (chartW <= 10 || chartH <= 10) return;

    // 提取有效数值以计算坐标系范围
    const allVals = [];
    data.forEach((d) => {
      if (d[series1.key] !== null && d[series1.key] !== undefined) allVals.push(Number(d[series1.key]));
      if (d[series2.key] !== null && d[series2.key] !== undefined) allVals.push(Number(d[series2.key]));
    });

    let minVal = allVals.length > 0 ? Math.min(...allVals) : 0;
    let maxVal = allVals.length > 0 ? Math.max(...allVals) : 100;

    if (minVal === maxVal) {
      minVal -= 5;
      maxVal += 5;
    }
    // 增加 10% 边距留白
    const valRange = maxVal - minVal;
    minVal = Math.floor(minVal - valRange * 0.1);
    maxVal = Math.ceil(maxVal + valRange * 0.1);

    const getX = (i) => pad.left + (data.length > 1 ? (i / (data.length - 1)) * chartW : chartW / 2);
    const getY = (val) => pad.top + chartH - ((val - minVal) / (maxVal - minVal)) * chartH;

    // 1. 绘制背景网格线与 Y 轴刻度
    const yGridCount = 4;
    ctx.font = '11px -apple-system, sans-serif';
    ctx.fillStyle = '#94a3b8';
    ctx.strokeStyle = '#f1f5f9';
    ctx.lineWidth = 1;

    for (let i = 0; i <= yGridCount; i++) {
      const v = minVal + ((maxVal - minVal) / yGridCount) * i;
      const y = getY(v);

      ctx.beginPath();
      ctx.moveTo(pad.left, y);
      ctx.lineTo(pad.left + chartW, y);
      ctx.stroke();

      ctx.textAlign = 'right';
      ctx.textBaseline = 'middle';
      ctx.fillText(v.toFixed(0) + series1.unit, pad.left - 8, y);
    }

    // 2. 绘制 X 轴时间刻度
    const xGridCount = Math.min(data.length, 5);
    ctx.textAlign = 'center';
    ctx.textBaseline = 'top';

    for (let i = 0; i < xGridCount; i++) {
      const idx = Math.floor((i / (xGridCount - 1 || 1)) * (data.length - 1));
      const x = getX(idx);
      const timeStr = formatShortTime(data[idx].received_at);
      ctx.fillText(timeStr, x, pad.top + chartH + 8);
    }

    // 3. 绘制折线与渐变填充辅助函数
    function drawSeries(series, isDashed = false) {
      const points = [];
      // 折线路径
      ctx.beginPath();
      ctx.strokeStyle = series.color;
      ctx.lineWidth = 2.5;
      if (isDashed) {
        ctx.setLineDash([4, 4]);
      } else {
        ctx.setLineDash([]);
      }

      let previousWasInvalid = true;
      data.forEach((d, i) => {
        const raw = d[series.key];
        if (raw === null || raw === undefined || !Number.isFinite(Number(raw))) {
          previousWasInvalid = true;
          return;
        }

        const point = { x: getX(i), y: getY(Number(raw)), val: Number(raw), idx: i };
        points.push(point);
        if (previousWasInvalid) {
          ctx.moveTo(point.x, point.y);
        } else {
          ctx.lineTo(point.x, point.y);
        }
        previousWasInvalid = false;
      });

      if (points.length === 0) return;
      ctx.stroke();
      ctx.setLineDash([]);

      // 数据圆点
      points.forEach((p) => {
        ctx.beginPath();
        ctx.arc(p.x, p.y, hoverIndex === p.idx ? 5.5 : 3.5, 0, Math.PI * 2);
        ctx.fillStyle = '#ffffff';
        ctx.fill();
        ctx.lineWidth = 2;
        ctx.strokeStyle = series.color;
        ctx.stroke();
      });
    }

    // 绘制两条折线
    drawSeries(series1, false);
    drawSeries(series2, true);

    // 4. 绘制悬停指示与 Tooltip
    if (hoverIndex >= 0 && hoverIndex < data.length) {
      const d = data[hoverIndex];
      const hX = getX(hoverIndex);

      // 垂线
      ctx.beginPath();
      ctx.strokeStyle = '#cbd5e1';
      ctx.setLineDash([3, 3]);
      ctx.moveTo(hX, pad.top);
      ctx.lineTo(hX, pad.top + chartH);
      ctx.stroke();
      ctx.setLineDash([]);

      // Tooltip 气泡
      const v1 = d[series1.key] !== null && d[series1.key] !== undefined ? `${Number(d[series1.key]).toFixed(2)} ${series1.unit}` : '--';
      const v2 = d[series2.key] !== null && d[series2.key] !== undefined ? `${Number(d[series2.key]).toFixed(2)} ${series2.unit}` : '--';
      const tText = formatTimestamp(d.received_at);
      const seqText = `Seq #${d.sequence_no}`;

      const lines = [
        `${tText} (${seqText})`,
        `${series1.name}: ${v1}`,
        `${series2.name}: ${v2}`
      ];

      ctx.font = '12px -apple-system, sans-serif';
      let boxW = 160;
      const boxH = 68;
      let boxX = hX + 10;
      if (boxX + boxW > width - 10) boxX = hX - boxW - 10;
      let boxY = pad.top + 10;

      ctx.fillStyle = 'rgba(15, 23, 42, 0.9)';
      ctx.beginPath();
      ctx.roundRect ? ctx.roundRect(boxX, boxY, boxW, boxH, 6) : ctx.rect(boxX, boxY, boxW, boxH);
      ctx.fill();

      ctx.fillStyle = '#f8fafc';
      ctx.textAlign = 'left';
      ctx.textBaseline = 'top';
      ctx.fillText(lines[0], boxX + 10, boxY + 8);

      ctx.fillStyle = '#fb923c';
      ctx.fillText(lines[1], boxX + 10, boxY + 28);

      ctx.fillStyle = '#60a5fa';
      ctx.fillText(lines[2], boxX + 10, boxY + 46);
    }

    // 绑定鼠标/触摸交互事件 (只绑定一次)
    if (!canvas._hasChartEvents) {
      canvas._hasChartEvents = true;

      const handleMove = (e) => {
        const chart = canvas._chartState;
        if (!chart) return;
        const cRect = canvas.getBoundingClientRect();
        const clientX = (e.touches && e.touches.length > 0) ? e.touches[0].clientX : e.clientX;
        // 容器宽度可能在两次绘制之间变化，先归一化到绘制时的坐标系
        const scaleX = cRect.width > 0 ? chart.width / cRect.width : 1;
        const mouseX = (clientX - cRect.left) * scaleX;

        if (mouseX < chart.padLeft || mouseX > chart.padLeft + chart.chartW) {
          chart.onHover(-1);
          return;
        }

        const ratio = (mouseX - chart.padLeft) / chart.chartW;
        const nearestIdx = Math.round(ratio * (chart.data.length - 1));
        if (nearestIdx >= 0 && nearestIdx < chart.data.length) {
          chart.onHover(nearestIdx);
        }
      };

      canvas.addEventListener('mousemove', handleMove);
      canvas.addEventListener('mouseleave', () => {
        if (canvas._chartState) canvas._chartState.onHover(-1);
      });
      canvas.addEventListener('touchmove', handleMove, { passive: true });
      canvas.addEventListener('touchend', () => {
        if (canvas._chartState) canvas._chartState.onHover(-1);
      });
    }
  }

  // =========================================================================
  // 自动轮询与交互事件绑定
  // =========================================================================

  function startAutoRefreshTimer() {
    if (state.timerId) clearInterval(state.timerId);
    state.countdown = state.refreshIntervalSeconds;
    updateTimerDisplay();

    state.timerId = setInterval(async () => {
      if (!state.autoRefresh) return;
      state.countdown--;
      if (state.countdown <= 0) {
        state.countdown = state.refreshIntervalSeconds;
        await refreshAll(false);
      }
      updateTimerDisplay();
    }, 1000);
  }

  function updateTimerDisplay() {
    if (state.autoRefresh) {
      el.refreshTimerLabel.textContent = `${state.countdown}s 自动刷新`;
    } else {
      el.refreshTimerLabel.textContent = '自动刷新已暂停';
    }
  }

  async function refreshAll(showButtonAnimation = true) {
    if (state.isFetching) return;
    state.isFetching = true;

    if (showButtonAnimation) {
      el.refreshBtn.style.opacity = '0.7';
      el.refreshBtn.querySelector('.btn-icon').style.transform = 'rotate(360deg)';
      el.refreshBtn.querySelector('.btn-icon').style.transition = 'transform 0.5s ease';
    }

    try {
      await loadGateways(true);
    } finally {
      state.isFetching = false;
      if (showButtonAnimation) {
        setTimeout(() => {
          el.refreshBtn.style.opacity = '1';
          el.refreshBtn.querySelector('.btn-icon').style.transform = 'none';
          el.refreshBtn.querySelector('.btn-icon').style.transition = 'none';
        }, 500);
      }
    }
  }

  function initEventListeners() {
    // 网关选择切换
    el.gatewaySelect.addEventListener('change', async (e) => {
      state.selectedGatewayId = e.target.value;
      state.selectedNodeId = null;
      updateGatewayUI();
      await loadNodes(false);
    });

    // 节点选择切换
    el.nodeSelect.addEventListener('change', async (e) => {
      state.selectedNodeId = e.target.value ? Number(e.target.value) : null;
      await loadNodeTelemetry();
    });

    // 立即刷新按钮
    el.refreshBtn.addEventListener('click', () => {
      state.countdown = state.refreshIntervalSeconds;
      refreshAll(true);
    });

    // 自动刷新开关
    el.autoRefreshToggle.addEventListener('change', (e) => {
      state.autoRefresh = e.target.checked;
      updateTimerDisplay();
    });

    // 历史条数选择按钮
    el.limitButtons.forEach((btn) => {
      btn.addEventListener('click', () => {
        el.limitButtons.forEach(b => b.classList.remove('active'));
        btn.classList.add('active');
        state.historyLimit = Number(btn.getAttribute('data-limit')) || 20;
        loadNodeTelemetry();
      });
    });

    // 关闭错误提示条
    el.dismissErrorBtn.addEventListener('click', () => {
      el.errorBanner.classList.add('hidden');
    });

    // 窗口尺寸变化时重新渲染图表
    window.addEventListener('resize', () => {
      if (state.history.length > 0) {
        renderCharts(state.history);
      }
    });
  }

  // =========================================================================
  // 初始化入口
  // =========================================================================

  async function init() {
    initEventListeners();
    await refreshAll(false);
    startAutoRefreshTimer();
  }

  // DOM 就绪后启动
  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', init);
  } else {
    init();
  }

})();
