# ext-transient-seat-v1 测试规范

## 范围

- Interface：`ext_transient_seat_manager_v1`，版本 1。
- Fixture：完整 Treeland 启动路径与真实 C Wayland 客户端。
- 覆盖等级：E / P。

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 |
| --- | --- | --- |
| 独立创建 | 创建两个 transient seat | 两个不同的 ready global，均可绑定为 wl_seat |
| 运行时配置 | 回读 SeatManager | seat 具有 cursor、layout、焦点窗口、事件过滤器，且不写入 saveConfig |
| 客户端释放 | 释放其中一个 transient resource | SeatManager 仅移除对应 seat，另一个保持有效 |
| 合成器移除 | 先移除 seat，再释放 resource | SeatManager 为空，失效 resource 可安全释放 |
| 断连清理 | 携带存活 transient seat 断连 | 第二个连接同步后 SeatManager 不再包含该 seat |

测试启用生产 keyboard-state-notify 监听，确认键盘组销毁时相应监听被同步解除。

## 已知边界

尚未覆盖内存分配失败的 denied 路径、临时 seat 上的实际输入事件和完整 compositor 重启。
