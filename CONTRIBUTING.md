# 开发协作说明

## 提交规范

提交标题使用：

```text
<type>(<scope>): <中文描述>
```

`type` 使用 `feat`、`fix`、`docs`、`refactor`、`perf`、`test`、`chore` 或 `ci`。描述使用动宾短语，不以句号结尾。

示例：

```text
feat(板级): 添加 GEC-GAGA 初始化
fix(触摸): 修正 GT911 坐标镜像
docs(验证): 补充 H0 外设验收记录
```

## 提交前检查

- 确认没有暂存 `build/`、`managed_components/` 或本地 `sdkconfig`。
- 运行 `git diff --check`。
- 使用 ESP-IDF v5.5.4 执行 `idf.py build`。
- 更新 `docs/validation_matrix.md`，记录目标、固件提交和验证结果。
- 硬件相关修改必须同步更新 `docs/hardware_baseline.md`。

## 硬件修改原则

未经串口日志、示波器或板卡资料确认的 GPIO、有效电平、I²C 地址和屏幕参数，不得作为最终配置提交。遇到硬件信息不足时，先补充证据，再修改板级驱动。
