# drv_uart.c 修改参考

以下是对 `drivers/drv_uart.c` 的修改, 添加 ESP32-C3 使用的 SCI0 通道。

## 1. 在文件头部添加 ESP32 设备定义和变量

在 `gLogDevice` 定义附近添加:

```c
/* ---- ESP32 WiFi 模块的 UART 设备 ---- */
static volatile bool gEsp32TxCplt = false;
static volatile bool gEsp32RxCplt = false;

static struct UartDev gEsp32Device = {
    .name    = "ESP32",
    .channel = 0,
    .Init    = UARTDrvInit,
    .Read    = UARTDrvRead,
    .Write   = UARTDrvWrite,
    .next    = NULL
};
```

## 2. 在 UartDevicesCreate() 中注册 ESP32 设备

```c
void UartDevicesCreate(void)
{
    UartDeviceInsert(&gLogDevice);
    gLogDevice.Init(&gLogDevice);
    
    /* 新增: ESP32 模块 */
    UartDeviceInsert(&gEsp32Device);
    gEsp32Device.Init(&gEsp32Device);
}
```

## 3. 在 UARTDrvInit() 的 switch 中修改 case 0:

```c
case 0:
{
    fsp_err_t err = g_uart0.p_api->open(g_uart0.p_ctrl, g_uart0.p_cfg);
    assert(FSP_SUCCESS == err);
    break;
}
```

## 4. 在 UARTDrvWrite() 的 switch 中修改 case 0:

```c
case 0:
{
    fsp_err_t err = g_uart0.p_api->write(g_uart0.p_ctrl, buf, length);
    assert(FSP_SUCCESS == err);
    gEsp32TxCplt = false;
    while (!gEsp32TxCplt);  /* 等待发送完成 */
    break;
}
```

## 5. 在 UARTDrvRead() 的 switch 中修改 case 0:

```c
case 0:
{
    fsp_err_t err = g_uart0.p_api->read(g_uart0.p_ctrl, buf, length);
    assert(FSP_SUCCESS == err);
    gEsp32RxCplt = false;
    while (!gEsp32RxCplt);  /* 等待接收完成 */
    break;
}
```

## 6. 添加 ESP32 UART 回调

在文件末尾添加:

```c
void esp32_uart_callback(uart_callback_args_t *p_args)
{
    switch (p_args->event)
    {
        case UART_EVENT_RX_COMPLETE:
        {
            gEsp32RxCplt = true;
            break;
        }
        case UART_EVENT_TX_COMPLETE:
        {
            gEsp32TxCplt = true;
            break;
        }
        case UART_EVENT_RX_CHAR:
        case UART_EVENT_TX_DATA_EMPTY:
        case UART_EVENT_ERR_PARITY:
        case UART_EVENT_ERR_FRAMING:
        case UART_EVENT_ERR_OVERFLOW:
        case UART_EVENT_BREAK_DETECT:
            break;
        default: break;
    }
}
```

## 7. 关键说明

**drv_esp32.c 中的 `esp32_uart_callback` 与 drv_uart.c 中的回调关系:**

`drv_esp32.c` 中的 `esp32_uart_callback` 使用了自己的 flag `gEsp32RxCplt`/`gEsp32TxCplt`
(在 drv_esp32.c 中声明为 `static volatile int`)。

而 `drv_uart.c` 中的回调使用的是 `gEsp32RxCplt`/`gEsp32TxCplt` (bool 类型)。

这两者需要统一。建议方案:
- **方案 A (推荐):** drv_uart.c 通过 FSP 注册回调, drv_esp32.c 通过 extern 引用 drv_uart.c 的 flag
- **方案 B:** drv_uart.c 只负责 Init, drv_esp32.c 自己直接调用 FSP API 并注册自己的回调

如果选择方案 B (更干净, 建议), drv_uart.c 中只需要:
- `case 0` 只在 Init 中调用 open
- Write 和 Read 的 case 0 可以保留为空 (由 drv_esp32.c 直接调用 g_uart0 API)

这样 drv_esp32.c 和 drv_uart.c 解耦, ESP32 通信完全由 drv_esp32.c 控制。
