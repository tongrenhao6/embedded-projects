# 咕嘎 AI 桌面宠物

基于小智 AI v2.3.0 的 ESP32-S3 桌宠项目，目标硬件为粤嵌 GEC-ESP32-S3 小智开发板。

## 项目结构

- `xiaozhi/`：小智 AI 固件源码。
- `PROJECT_PLAN.md`：项目规划、阶段目标和验收标准。
- `docs/hardware_baseline.md`：硬件实测基线与串口证据。
- `docs/h0_bringup_plan.md`：H0 分阶段硬件冒烟测试与风险控制方案。
- `docs/validation_matrix.md`：阶段验证矩阵和性能记录。

## 已确认环境

- 目标芯片：ESP32-S3 revision v0.2。
- Flash：16 MB。
- PSRAM：8 MB。
- ESP-IDF：v5.5.4。
- 当前开发串口：`COM18`（CH340K）。

## 开发约束

1. 先完成 H0 硬件冒烟测试，再固化 GPIO、I²C 地址和显示参数。
2. 不把 `xiaozhi/build/` 或 `xiaozhi/managed_components/` 等生成内容提交到 Git。
3. `xiaozhi/dependencies.lock` 用于记录组件解析结果，应纳入版本控制。
4. LVGL 对象只能由 UI 任务或受保护的 LVGL 上下文修改。
5. 每次板级代码修改后，使用 ESP-IDF 环境执行构建，并将结果记录到验证矩阵。

## 构建前置

在 PowerShell 中激活 ESP-IDF：

```powershell
& "$env:IDF_PATH\export.ps1"
```

进入固件目录并确认目标：

```powershell
Set-Location .\xiaozhi
idf.py set-target esp32s3
idf.py build
```

烧录和串口监控前，先确认开发板串口号和硬件基线已更新：

```powershell
idf.py -p COM18 flash monitor
```

## Git 工作流

提交采用 Conventional Commits 格式，例如：

```text
docs(规划): 完善硬件验证与运行时架构
feat(板级): 添加 GEC-GAGA ESP32-S3 适配
fix(显示): 修正 ST7796 色彩顺序
```

提交前至少执行：

```powershell
git status
git diff --check
```

## 当前状态

仓库已完成初始化，源码和规划文档已加入暂存区。H0 尚未完成；当前串口上的固件仍是未知旧固件，不能作为目标板适配结果。
