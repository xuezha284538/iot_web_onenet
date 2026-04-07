#ifndef __CONFIG_H
#define __CONFIG_H

// ==================== WiFi配置 ====================
#define WIFI_SSID       "hello"         // WiFi名称
#define WIFI_PASS       "12345678"      // WiFi密码

// ==================== OneNET平台配置 ====================
#define ONENET_SERVER_IP     "mqtts.heclouds.com"  // OneNET服务器地址
#define ONENET_SERVER_PORT   1883                    // OneNET服务器端口
#define ONENET_PRODUCT_ID    "K3GGK1Sj7Q"           // 产品ID
#define ONENET_DEVICE_NAME   "stm"                  // 设备名称
#define ONENET_AUTH_INFO     "version=2018-10-31&res=products%2FK3GGK1Sj7Q%2Fdevices%2Fstm&et=1787733887&method=md5&sign=7pieyIabazlMcHuGGmZ8qQ%3D%3D"  // 鉴权信息

// ==================== 系统参数 ====================
#define SENSOR_READ_INTERVAL    100     // 传感器读取间隔(ms)
#define DATA_SEND_INTERVAL      1000    // 数据发送间隔(ms)
#define DISPLAY_REFRESH_MS      200     // 显示刷新间隔(ms)
#define CONNECTION_RETRY_MS     1000    // 连接重试间隔(ms)

// ==================== 自动控制阈值 ====================
#define SMOKE_ALARM_THRESHOLD   1500    // 烟雾报警阈值
#define TEMP_FAN_THRESHOLD      35      // 温度过高开启风扇阈值

// ==================== 硬件使能配置 ====================
#define ENABLE_LED          0   // LED使能（0-禁用，1-启用）
#define ENABLE_BUZZER       0   // 蜂鸣器使能（0-禁用，1-启用）
#define ENABLE_KEY          0   // 按键使能（0-禁用，1-启用）
#define ENABLE_WATCHDOG     0   // 看门狗使能（0-禁用，1-启用）

// ==================== 看门狗配置 ====================
#define WATCHDOG_TIMEOUT    5000    // 看门狗超时时间(ms)

#endif /* __CONFIG_H */
