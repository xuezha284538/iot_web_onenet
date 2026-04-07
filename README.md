# iot_web 网页web网址 https://xuezha284538.github.io/iot_web_onenet/
# 智慧机房控制系统

## 项目简介

本项目是一个基于STM32微控制器的智慧机房控制系统，通过WiFi模块连接OneNET云平台，实现对机房环境的实时监控和控制。系统包含三个客户端：微信小程序、Web网页版和STM32嵌入式端。

## 系统架构

```

   
┌─────────────────┐     WiFi/ MQTT      ┌─────────────────┐
│   微信小程序      │ ◄─────────────────► │   OneNET云平台   │
│  miniprogram/   │                     │                 │──────────┬  
└─────────────────┘                     └────────┬────────┘          │ MQTT/HTTP
                                                 ^                   | 
    ┌────────────────┐                           │                   |
    │   Web网页版     │         MQTT              │                   |
    │  src/          │◄──────────────────────────|                   |
    │  index2.html   │                                               |
    └────────────────┘                                               |
                                                                     |
                                                                     |
             ┌───────────────┐                                       |
             │   STM32端      │                                      |      
             │    stm32/     │                                       |    
             │ - 传感器采集   │<- ─────────────────────────────── <-- -|
             │ - 数据上传     │
             │ - 自动控制     │
             └───────────────┘
                       


```

## 项目结构

```
├── stm32/                    # STM32嵌入式项目
│   ├── Hardware/              # 硬件驱动
│   │   ├── BH1750.c/h        # 光照传感器驱动
│   │   ├── DHT11.c/h          # 温湿度传感器驱动
│   │   ├── ESP8266.c/h        # WiFi模块驱动
│   │   ├── MQ2.c/h            # 烟雾传感器驱动
│   │   ├── OLED.c/h           # OLED显示屏驱动
│   │   ├── LED.c/h            # LED驱动
│   │   ├── Buzzer.c/h         # 蜂鸣器驱动
│   │   ├── Key.c/h            # 按键驱动
│   │   ├── Timer.c/h          # 定时器驱动
│   │   └── UART.c/h           # 串口驱动
│   ├── Library/               # STM32标准库
│   ├── Start/                 # 启动文件
│   ├── System/                # 系统功能（延时函数）
│   ├── User/                  # 用户代码
│   │   ├── main_onenet.c      # 主程序（OneNET版本）
│   │   ├── onenet.c/h         # OneNET云平台驱动
│   │   ├── MqttKit.c/h        # MQTT协议实现
│   │   └── config.h           # 配置文件
│   ├── DebugConfig/           # 调试配置
│   ├── Listings/              # 编译列表
│   ├── Objects/               # 编译输出
│   └── Project.uvprojx        # Keil项目文件
│
│

│
├── index.html               # Web主页面
└── README.md                  # 项目说明文档
```

## 功能特性

### 1. STM32端功能

- **传感器数据采集**
  - 温度、湿度监测（DHT11）
  - 光照强度监测（BH1750）
  - 烟雾浓度监测（MQ2）

- **显示功能**
  - OLED显示屏实时显示传感器数据
  - 显示连接状态和数据上传状态

- **网络功能**
  - ESP8266 WiFi模块连接
  - OneNET云平台数据上传
  - MQTT协议通信
  - 断线自动重连

- **自动控制**
  - 温度过高自动开启风扇
  - 烟雾超标自动报警
  - 设备状态自动上报


### 2. Web端功能

- **数据展示**
  - 实时传感器数据
  - 历史数据查询
  - 数据统计分析
  - 数据可视化图表

- **设备管理**
  - 设备列表展示
  - 设备状态监控
  - 设备控制

- **响应式设计**
  - 适配桌面端和移动端
  - 友好的用户界面

## 硬件连接

### 引脚配置

| 传感器/模块 | GPIO引脚 | 说明 |
|------------|----------|------|
| DHT11 | PB14 | 温湿度数据 |
| BH1750 | PB6/PB7 | I2C接口（SCL/SDA） |
| MQ2 | PA0 | ADC模拟输入 |
| OLED | PB8/PB9 | I2C接口（SCL/SDA） |
| ESP8266 | PA9/PA10 | USART1（TX/RX） |
| LED1 | PA1 | 指示灯1 |
| LED2 | PA2 | 指示灯2 |
| Buzzer | PB12 | 蜂鸣器 |
| Key1 | PB1 | 按键1 |
| Key2 | PB11 | 按键2 |

## 开发环境

### STM32开发

- **IDE**: Keil MDK
- **芯片**: STM32F103C8T6
- **库**: STM32标准库

### Web开发
- **框架**: Vue.js 3
- **构建工具**: Vite
- **图表库**: ECharts

## 使用说明

### 1. STM32端

1. 使用Keil打开 `stm32/Project.uvprojx`
2. 配置WiFi账号密码（修改 `User/config.h`）
3. 配置OneNET平台信息（产品ID、设备名称、鉴权信息）
4. 编译并下载到STM32芯片
5. 上电后系统自动连接WiFi和OneNET

### 2. Web端

```bash
# 安装依赖
npm install

# 启动开发服务器
npm run dev

# 构建生产版本
npm run build
```

## 配置说明

### OneNET配置

在 `stm32/User/config.h` 中修改：

```c
// WiFi配置
#define WIFI_SSID       "your_wifi_name"     // WiFi名称
#define WIFI_PASS       "your_wifi_password" // WiFi密码

// OneNET配置
#define ONENET_SERVER_IP     "mqtts.heclouds.com"
#define ONENET_SERVER_PORT   1883
#define ONENET_PRODUCT_ID    "your_product_id"
#define ONENET_DEVICE_NAME   "your_device_name"
#define ONENET_AUTH_INFO     "your_auth_info"
```

## 技术栈

- **嵌入式**: C语言、STM32标准库
- **通信协议**: MQTT、HTTP、AT指令
- **云平台**: OneNET
- **Web前端**: HTML5、CSS3、JavaScript、Vue.js

## 项目版本

- **版本**: 2026
- **作者**: 林科大学渣
- **版权**: © 2026 智慧机房

## 注意事项

1. STM32项目需要先在Keil中重新配置路径
2. WiFi和OneNET配置需要根据实际情况修改
3. 硬件连接请参考引脚配置表

## 联系方式

- 微信号: qq2845382275
- 邮箱: 2845382275@qq.com

---

<p align="center">© 2026 智慧机房 | 林科大学渣</p>
