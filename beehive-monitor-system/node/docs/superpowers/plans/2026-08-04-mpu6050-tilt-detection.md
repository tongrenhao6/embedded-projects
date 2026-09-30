# MPU6050 倾倒检测实现计划

> **面向 AI 代理的工作计划：** 本计划用于后续逐项实现，每完成一个步骤都要运行对应验证；当前不改变 LoRa 帧长度。

**目标：** 在节点 ESP32-S3 上接入 GY-521/MPU6050，通过加速度计的重力方向判断箱体是否倾倒，并把结果随现有传感器帧上报网关。

**方案：** MPU6050 与 SHT30 共用 I2C0（SDA GPIO6、SCL GPIO7），设备地址使用 AD0 接地后的 0x68。模块固定在箱体刚性底板，元件面朝上，Z 轴朝上。按加速度向量计算相对竖直方向的倾角；倾倒确认阈值 45 度，恢复阈值 35 度，连续满足 3 次才改变状态。第一版不使用 INT、XDA、XCL，也不扩展 26 字节协议帧，只新增 status_flags 位。

**技术栈：** ESP-IDF 5.5.4 新版 `driver/i2c_master.h` API、MPU6050 寄存器读写、FreeRTOS 周期采样、现有 `frame_codec` 和 `lora_transport`。

## 文件结构

- 创建：`F:/0Node/Nodeproject/components/tilt_sensor/CMakeLists.txt`
- 创建：`F:/0Node/Nodeproject/components/tilt_sensor/include/tilt_sensor.h`
- 创建：`F:/0Node/Nodeproject/components/tilt_sensor/src/tilt_sensor.c`
- 创建：`F:/0Node/Nodeproject/components/tilt_sensor/test/test_tilt_sensor.c`
- 修改：`F:/0Node/Nodeproject/components/frame_codec/include/frame_codec.h`，增加倾倒有效和倾倒告警状态位。
- 修改：`F:/0Node/Nodeproject/components/frame_codec/src/frame_codec.c` 及其测试（仅当状态位需要 codec 覆盖时）。
- 修改：`F:/0Node/Nodeproject/main/main.c`，初始化和周期读取倾倒状态，将状态位合并到现有传感器帧。
- 修改：`F:/0Node/Nodeproject/docs/protocol/sensor_frame_v1.md`，记录新增 status flag 语义。

## 任务 1：定义并验证纯算法接口

**文件：** `components/tilt_sensor/include/tilt_sensor.h`、`components/tilt_sensor/src/tilt_sensor.c`、`components/tilt_sensor/test/test_tilt_sensor.c`

- [x] 编写失败测试：输入理想重力向量 `(0, 0, 1g)` 得到 0 度；输入 90 度侧放向量得到约 90 度；输入无效零向量返回错误。
- [x] 运行主机测试，确认失败原因是算法接口尚未实现。
- [x] 实现不依赖硬件的 `tilt_sensor_compute_angle_deci_degrees()` 和带滞回/连续计数的状态更新函数，角度使用 `atan2`，输入使用有符号加速度原始值或已换算的 mg，避免整数溢出。
- [x] 再次运行 `test_tilt_sensor`，确认角度误差、阈值和连续采样规则通过。

## 任务 2：实现 MPU6050 I2C 驱动

**文件：** `components/tilt_sensor/CMakeLists.txt`、`components/tilt_sensor/src/tilt_sensor.c`

- [x] 先写设备初始化/读取测试所需的寄存器常量和接口定义，固定地址 0x68、I2C 100 kHz、超时 100 ms。
- [x] 使用共享 I2C 总线句柄初始化 MPU6050：验证 `WHO_AM_I` 为 0x68，退出睡眠，设置加速度量程为 ±2 g，陀螺仪量程保持默认或关闭读取。
- [x] 提供 `tilt_sensor_init(i2c_master_bus_handle_t bus)` 和 `tilt_sensor_read()`，由调用方传入现有 SHT30 总线，禁止组件重复创建 GPIO6/7 总线。
- [x] 按数据手册连续读取 `ACCEL_XOUT_H` 起始的 6 个字节，检查每次 I2C 返回值，并把原始值换算为 mg（±2 g 为 16384 LSB/g）。
- [x] 初始化失败时记录地址、WHO_AM_I 和 `esp_err_t`；读取失败不改变上一份有效倾倒状态。

## 任务 3：接入节点采样与状态上报

**文件：** `main/main.c`、`components/frame_codec/include/frame_codec.h`、`docs/protocol/sensor_frame_v1.md`

- [x] 增加 `FRAME_CODEC_STATUS_TILT_VALID` 和 `FRAME_CODEC_STATUS_TILTED`，保留现有箱内/箱外传感器位含义。
- [x] 在 `sensor_service` 初始化成功后，把 I2C 总线句柄提供给 tilt 组件并初始化 MPU6050；不得改变 SHT30 的地址探测和测量时序。
- [x] 每次传感器发送周期读取一次加速度并更新滞回状态；初始化或读取失败时清除 `TILT_VALID`，并保留温湿度发送。
- [x] 组帧时将倾倒有效/告警位与箱内传感器有效位按位或合并；日志输出 `tilt angle`、`tilt state` 和错误原因。
- [x] 文档明确 status bit 2 为倾倒数据有效、bit 3 为当前倾倒告警，接收端在 bit 2 清零时必须忽略 bit 3。

## 任务 4：主机测试与实机验证

**文件：** `components/frame_codec/test/test_frame_codec.c`、`components/tilt_sensor/test/test_tilt_sensor.c`

- [x] 运行 `test_tilt_sensor`、`test_frame_codec` 和 `test_sht30_codec`，覆盖正常、倾倒、恢复、无效向量及 CRC 不变等场景。
- [x] 在 `F:/0Node/Nodeproject` 运行 ESP-IDF 5.5.4 build，确认 ESP32-S3 固件构建通过且没有新增编译警告。
- [ ] 用户接线：MPU6050 `VCC -> 3V3`、`GND -> GND`、`SDA -> GPIO6`、`SCL -> GPIO7`、`AD0 -> GND`；`INT/XDA/XCL` 不接。上电前确认不把 5V 接入 I2C 信号脚。
- [ ] 烧录后保持箱体正常姿态，确认日志显示角度接近 0 度且 status 为有效/未倾倒；再将箱体缓慢侧倾超过 45 度并保持约 3 个采样周期，确认告警位变为 1；恢复到小于 35 度后确认告警清除。
- [ ] 通过网关接收端确认帧仍为 26 字节、CRC 正常，且仅 status_flags 发生预期变化。

## 风险与边界

- 该方案检测的是“当前静态姿态倾斜”，不是瞬时碰撞或自由落体；箱体移动时可能暂时产生无效判断，因此采用连续采样和滞回。
- 若实际安装时模块轴向与本计划不同，必须在软件中交换/取反轴，不能仅调整阈值掩盖方向错误。
- MPU6050 模块的 I2C 上拉可能连接到 VCC；本项目必须给模块供 3.3 V，避免 GPIO6/7 被拉到 5 V。
