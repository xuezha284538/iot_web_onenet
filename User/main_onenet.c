/**
  ******************************************************************************
  * @file    main_onenet.c
  * @author  STM32 Project
  * @version V2.0
  * @date    2025
  * @brief   主程序 - 传感器数据采集 + OneNET云平台上传 + 自动控制
  ******************************************************************************
  */

#include "stm32f10x.h"
#include "config.h"  // 配置头文件
#include "Delay.h"
#include "OLED.h"
#include "DHT11.h"
#include "BH1750.h"
#include "MQ2.h"
#include "LED.h"
#include "Buzzer.h"
#include "ESP8266.h"
#include "onenet.h"
#include "Timer.h"  // 定时器中断
#include <string.h>
#include <stdio.h>

// ==================== 配置定义 ====================
// WiFi和OneNET配置已移至config.h

// 系统参数已移至config.h

// 自动控制阈值已移至config.h

// ==================== 数据结构 ====================
// 连接状态枚举（用于非阻塞连接状态机）
typedef enum {
    CONN_STATE_IDLE = 0,          // 空闲/已连接
    CONN_STATE_WIFI_TEST,         // 测试AT指令
    CONN_STATE_WIFI_SET_MODE,     // 设置STA模式
    CONN_STATE_WIFI_CONNECT,      // 连接WiFi热点
    CONN_STATE_WIFI_WAIT,         // 等待WiFi连接完成
    CONN_STATE_ONENET_CLOSE_TCP,  // 关闭旧TCP连接
    CONN_STATE_ONENET_START_TCP,  // 建立TCP连接
    CONN_STATE_ONENET_MQTT_CONN, // MQTT连接
} ConnectionState_t;

// 传感器数据结构
typedef struct {
    uint8_t temperature;    // 温度 (°C)
    uint8_t humidity;       // 湿度 (%RH)
    uint16_t light;         // 光照 (lux)
    uint16_t smoke;         // 烟雾值
    uint8_t valid;          // 数据是否有效
} SensorData_t;

// 系统状态结构
typedef struct {
    uint8_t wifi_connected;     // WiFi连接状态
    uint8_t onenet_connected;   // OneNET连接状态
    uint32_t packet_sent;       // 发送数据包计数
    uint32_t error_count;       // 错误计数
    uint32_t reconnect_count;   // 重连次数统计
} SystemStatus_t;

// 后台连接控制结构
typedef struct {
    ConnectionState_t state;        // 当前连接状态
    uint32_t last_action_time;      // 上次操作时间
    uint8_t retry_count;            // 重试计数
    uint8_t is_connecting;          // 是否正在连接中
} ConnectionControl_t;

// ==================== 全局变量 ====================
SensorData_t sensor_data = {0};
SensorData_t last_upload_data = {0};  // 上次上传的数据（用于检测变化）
SystemStatus_t sys_status = {0};
ConnectionControl_t conn_ctrl = {0};   // 后台连接控制

// 自动控制状态
uint8_t auto_control_enabled = 1;  // 自动控制使能

// 数据变化标志
uint8_t data_changed = 0;  // 数据是否发生变化

// 时间戳
volatile uint32_t system_tick = 0;

// 最小上报间隔(ms) - 防止频繁上报
#define MIN_UPLOAD_INTERVAL  1000  // 至少5秒上报一次
#define FORCE_UPLOAD_INTERVAL 1000  // 强制上报间隔5秒（即使数据没变化）
uint32_t last_upload_time = 0;

// ==================== 函数声明 ====================
void System_Init(void);
void ReadSensors(void);
void AutoControl(void);
void SendToCloud(void);
void UpdateDisplay(void);
uint8_t ConnectWiFi(void);
uint8_t CheckWiFiStatus(void);  // 检查WiFi状态
uint8_t ConnectToOneNET(void);
void ShowStartupScreen(void);
void UpdateControlOutputs(void);
void BackgroundConnect(void);   // 后台连接（非阻塞）

// ==================== 主函数 ====================
int main(void)
{
    System_Init();
    ShowStartupScreen();
    Delay_ms(500);  // 快速启动，不等待太久
    
    OLED_Clear();
    
    // 上电立即开始后台连接WiFi和OneNET
    conn_ctrl.state = CONN_STATE_WIFI_TEST;  // 从测试AT指令开始
    conn_ctrl.is_connecting = 1;
    conn_ctrl.retry_count = 0;
    conn_ctrl.last_action_time = system_tick;
    
    // 主循环变量
    uint32_t last_sensor_read = 0;
    uint32_t last_data_send = 0;
    uint32_t last_display_update = 0;
    uint32_t last_connection_check = 0;
    uint32_t last_auto_control = 0;
    uint32_t last_background_connect = 0;  // 后台连接时间戳
    
    // 主循环
    while(1)
    {
        // 1. 后台连接（非阻塞）- 每3秒执行一次连接操作
        if(system_tick - last_background_connect >= 1000 || conn_ctrl.is_connecting)
        {
            last_background_connect = system_tick;
            BackgroundConnect();
        }
        
        // 2. 定期检查连接状态（每10秒）
        if(system_tick - last_connection_check >= CONNECTION_RETRY_MS)
        {
            last_connection_check = system_tick;
            
            // 如果已连接，检查是否仍然在线
            if(sys_status.wifi_connected && !conn_ctrl.is_connecting)
            {
                if(!CheckWiFiStatus())
                {
                    // WiFi已断开，重新开始连接
                    sys_status.wifi_connected = 0;
                    sys_status.onenet_connected = 0;
                    conn_ctrl.state = CONN_STATE_WIFI_TEST;
                    conn_ctrl.is_connecting = 1;
                    conn_ctrl.retry_count = 0;
                    sys_status.error_count++;
                }
            }
            
            // 如果未连接且没有在连接中，开始连接
            if(!sys_status.wifi_connected && !conn_ctrl.is_connecting)
            {
                conn_ctrl.state = CONN_STATE_WIFI_TEST;
                conn_ctrl.is_connecting = 1;
                conn_ctrl.retry_count = 0;
            }
            
            // 如果WiFi已连接但OneNET未连接，且不在连接过程中，开始OneNET连接
            if(sys_status.wifi_connected && !sys_status.onenet_connected && !conn_ctrl.is_connecting)
            {
                conn_ctrl.state = CONN_STATE_ONENET_CLOSE_TCP;
                conn_ctrl.is_connecting = 1;
                conn_ctrl.retry_count = 0;
            }
            
            // 检查OneNET连接状态
            if(sys_status.wifi_connected && sys_status.onenet_connected && !conn_ctrl.is_connecting)
            {
                // 发送心跳包或检查连接
                // 这里可以添加心跳检测逻辑
            }
        }
        
        // 3. 读取传感器数据
        if(system_tick - last_sensor_read >= SENSOR_READ_INTERVAL)
        {
            last_sensor_read = system_tick;
            ReadSensors();
        }
        
        // 4. 自动控制逻辑
       /* if(system_tick - last_auto_control >= 500)  // 每500ms检查一次
        {
            last_auto_control = system_tick;
            AutoControl();
        }*/
        
        // 5. 发送数据到OneNET云平台
        if(system_tick - last_data_send >= DATA_SEND_INTERVAL)
        {
            last_data_send = system_tick;
            if(sys_status.onenet_connected && sensor_data.valid && !conn_ctrl.is_connecting)
            {
                SendToCloud();
            }
        }
        
        // 6. 更新显示
        if(system_tick - last_display_update >= DISPLAY_REFRESH_MS)
        {
            last_display_update = system_tick;
            UpdateDisplay();
        }
        
        // 7. 更新控制输出
       // UpdateControlOutputs();
        
        // 8. 喂狗
        #if ENABLE_WATCHDOG
            IWDG_ReloadCounter();  // 喂狗
        #endif
        
        Delay_ms(10);
        system_tick += 10;
    }
}

// ==================== 系统初始化 ====================
void System_Init(void)
{
    OLED_Init();
    BH1750_Init();
    MQ2_Init();
    
    // 根据配置决定是否初始化硬件
    #if ENABLE_LED
        LED_Init();
    #endif
    
    #if ENABLE_BUZZER
        Buzzer_Init();
    #endif
    
    #if ENABLE_WATCHDOG
        // 初始化独立看门狗
        IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);
        IWDG_SetPrescaler(IWDG_Prescaler_32);  // 32分频
        IWDG_SetReload(WATCHDOG_TIMEOUT / 100);  // 设置重装载值
        IWDG_ReloadCounter();  // 喂狗
        IWDG_Enable();  // 使能看门狗
    #endif
    
    ESP8266_Init();
    TIM3_Init();  // 初始化定时器（用于OneNET重连）
    
    memset(&sensor_data, 0, sizeof(SensorData_t));
    memset(&sys_status, 0, sizeof(SystemStatus_t));
    
    // 初始化控制状态
    OneNet_SetControl(0, 0, 0);
    
    system_tick = 0;
}

// ==================== 启动画面 ====================
void ShowStartupScreen(void)
{
    OLED_Clear();
    OLED_ShowString(1, 1, "================");
    OLED_ShowString(2, 1, "  OneNET Cloud  ");
    OLED_ShowString(3, 1, "  Data Sender   ");
    OLED_ShowString(4, 1, "================");
}

// ==================== 连接WiFi热点 ====================
uint8_t ConnectWiFi(void)
{
    // 测试AT指令
    if(!ESP8266_Test()) {
        return 0;
    }
    Delay_ms(100);
    
    // 设置STA模式
    if(!ESP8266_SetMode(ESP8266_MODE_STA)) {
        return 0;
    }
    Delay_ms(100);
    
    // 连接WiFi热点
    if(!ESP8266_ConnectWiFi(WIFI_SSID, WIFI_PASS)) {
        return 0;
    }
    
    return 1;
}

// ==================== 检查WiFi连接状态 ====================
uint8_t CheckWiFiStatus(void)
{
    // 发送AT指令检查WiFi状态
    ESP8266_ClearBuffer();
    ESP8266_Clear();  // P9修复：同时清空esp8266_buf
    ESP8266_SendString("AT+CIPSTATUS\r\n");
    Delay_ms(100);
    
    // 检查响应中是否有 "STATUS:3" (已连接并获取IP)
    if(strstr((char*)esp8266_rx_buf, "STATUS:3") != NULL)
    {
        return 1;  // WiFi已连接且获取了IP
    }
    else if(strstr((char*)esp8266_rx_buf, "STATUS:2") != NULL)  // 已连接但未获取IP
    {
        return 1;  // 仍然认为已连接，等待获取IP
    }
    else if(strstr((char*)esp8266_rx_buf, "STATUS:4") != NULL ||  // 已断开
            strstr((char*)esp8266_rx_buf, "STATUS:5") != NULL)     // 未连接
    {
        return 0;  // 未连接
    }
    else if(strstr((char*)esp8266_rx_buf, "ERROR") != NULL)
    {
        // AT指令错误，可能ESP8266状态异常
        return 0;
    }
    
    // 如果无法确定，假设仍然连接（避免频繁重连）
    return sys_status.wifi_connected;  // 保持当前状态
}

// ==================== 连接OneNET云平台 ====================
uint8_t ConnectToOneNET(void)
{
    // 连接OneNET服务器（不显示OLED信息）
    _Bool result = OneNet_DevLink();
    
    if(result == 0)
    {
        return 1;  // 连接成功
    }
    else
    {
        return 0;  // 连接失败
    }
}

// ==================== 后台连接（非阻塞状态机）====================
void BackgroundConnect(void)
{
    if(!conn_ctrl.is_connecting) return;  // 不在连接状态，直接返回
    
    // 检查是否需要等待（防止操作太快）
    if(system_tick - conn_ctrl.last_action_time < 100) return;  // 至少间隔100ms
    
    uint8_t result;
    
    switch(conn_ctrl.state)
    {
        case CONN_STATE_WIFI_TEST:
            // 测试AT指令
            result = ESP8266_Test();
            conn_ctrl.last_action_time = system_tick;
            if(result)
            {
                conn_ctrl.state = CONN_STATE_WIFI_SET_MODE;  // 成功，下一步
                conn_ctrl.retry_count = 0;
            }
            else
            {
                conn_ctrl.retry_count++;
                if(conn_ctrl.retry_count > 5)  // 重试5次失败
                {
                    // 重置ESP8266
                    ESP8266_Reset();
                    conn_ctrl.state = CONN_STATE_WIFI_TEST;  // 重头开始
                    conn_ctrl.retry_count = 0;
                }
                // 否则继续重试当前步骤
            }
            break;
            
        case CONN_STATE_WIFI_SET_MODE:
            // 设置STA模式
            result = ESP8266_SetMode(ESP8266_MODE_STA);
            conn_ctrl.last_action_time = system_tick;
            if(result)
            {
                conn_ctrl.state = CONN_STATE_WIFI_CONNECT;
                conn_ctrl.retry_count = 0;
            }
            else
            {
                conn_ctrl.retry_count++;
                if(conn_ctrl.retry_count > 10)
                {
                    // 重置ESP8266
                    ESP8266_Reset();
                    conn_ctrl.state = CONN_STATE_WIFI_TEST;  // 重新开始
                    conn_ctrl.retry_count = 0;
                }
            }
            break;
            
        case CONN_STATE_WIFI_CONNECT:
            // 连接WiFi热点
            result = ESP8266_ConnectWiFi(WIFI_SSID, WIFI_PASS);
            conn_ctrl.last_action_time = system_tick;
            if(result)
            {
                sys_status.wifi_connected = 1;
                conn_ctrl.state = CONN_STATE_WIFI_WAIT;  // 等待连接完成
                conn_ctrl.retry_count = 0;
            }
            else
            {
                conn_ctrl.retry_count++;
                if(conn_ctrl.retry_count > 10)
                {
                    // 重置ESP8266
                    ESP8266_Reset();
                    conn_ctrl.state = CONN_STATE_WIFI_TEST;  // 重新开始
                    conn_ctrl.retry_count = 0;
                }
            }
            break;
            
        case CONN_STATE_WIFI_WAIT:
            // 等待WiFi连接完成（给ESP8266一些时间）
            conn_ctrl.last_action_time = system_tick;
            conn_ctrl.state = CONN_STATE_ONENET_CLOSE_TCP;  // 进入OneNET连接阶段
            break;
            
        case CONN_STATE_ONENET_CLOSE_TCP:
            // 关闭旧TCP连接
            ESP8266_CloseTCP();
            conn_ctrl.last_action_time = system_tick;
            conn_ctrl.state = CONN_STATE_ONENET_START_TCP;
            break;
            
        case CONN_STATE_ONENET_START_TCP:
            // 建立TCP连接到OneNET服务器
            result = ESP8266_StartTCP(ONENET_SERVER_IP, ONENET_SERVER_PORT);
            conn_ctrl.last_action_time = system_tick;
            if(result)
            {
                conn_ctrl.state = CONN_STATE_ONENET_MQTT_CONN;
                conn_ctrl.retry_count = 0;
            }
            else
            {
                conn_ctrl.retry_count++;
                if(conn_ctrl.retry_count > 5)
                {
                    conn_ctrl.state = CONN_STATE_WIFI_CONNECT;  // TCP失败，重新连WiFi
                    conn_ctrl.retry_count = 0;
                }
            }
            break;
            
        case CONN_STATE_ONENET_MQTT_CONN:
            // MQTT连接
            result = OneNet_DevLink();
            conn_ctrl.last_action_time = system_tick;
            if(result == 0)  // 成功
            {
                sys_status.onenet_connected = 1;
                conn_ctrl.is_connecting = 0;  // 连接完成！
                conn_ctrl.state = CONN_STATE_IDLE;
                data_changed = 1;  // 连接成功后立即上报一次数据
                
                // 统计重连次数（如果不是第一次连接）
                if(sys_status.reconnect_count > 0 || sys_status.packet_sent > 0)
                {
                    sys_status.reconnect_count++;
                }
            }
            else
            {
                conn_ctrl.retry_count++;
                if(conn_ctrl.retry_count > 3)
                {
                    conn_ctrl.state = CONN_STATE_ONENET_CLOSE_TCP;  // 重试TCP连接
                    conn_ctrl.retry_count = 0;
                }
            }
            break;
            
        default:
            conn_ctrl.is_connecting = 0;
            conn_ctrl.state = CONN_STATE_IDLE;
            break;
    }
}

// ==================== 读取传感器数据 ====================
void ReadSensors(void)
{
    static uint8_t dht_timer = 0;
    static uint16_t light_accum = 0;
    static uint16_t smoke_accum = 0;
    static uint8_t sample_count = 0;
    
    // 读取DHT11 (每1000ms)
    if(++dht_timer >= 8)
    {
        dht_timer = 0;
        DHT11_ReadData(&sensor_data.temperature, &sensor_data.humidity);
    }
    
    // 累加光照和烟雾值
    light_accum += (uint16_t)BH1750_ReadLightIntensity();
    smoke_accum += MQ2_Read();
    sample_count++;
    
    // 每5次采样取平均
    if(sample_count >= 5)
    {
        sensor_data.light = light_accum / 5;
        sensor_data.smoke = smoke_accum / 5;
        light_accum = 0;
        smoke_accum = 0;
        sample_count = 0;
        sensor_data.valid = 1;
        
        // 更新OneNET的全局传感器变量
        sensor_temp = sensor_data.temperature;
        sensor_humi = sensor_data.humidity;
        sensor_light = sensor_data.light;
        sensor_smoke = sensor_data.smoke;
        
        // 检测数据是否发生变化
        if(sensor_data.temperature != last_upload_data.temperature ||
           sensor_data.humidity != last_upload_data.humidity ||
           sensor_data.light != last_upload_data.light ||
           sensor_data.smoke != last_upload_data.smoke)
        {
            data_changed = 1;  // 数据有变化，需要上报
        }
    }
}

// ==================== 自动控制逻辑 ====================
void AutoControl(void)
{
   /* if(!auto_control_enabled || !sensor_data.valid) {
        return;
    }
    
    uint8_t fan, valve, alarm;
    OneNet_GetControl(&fan, &valve, &alarm);
    
    // 场景1: 烟雾超标报警 - 烟雾 > 1500
    if(sensor_data.smoke > SMOKE_ALARM_THRESHOLD)
    {
        alarm = 1;  // 开启报警器
        fan = 1;    // 开启风扇
    }
    else
    {
        // 如果烟雾正常，只根据云平台控制报警器
    }
    
    // 场景2: 温度过高降温 - 温度 > 35°C
    if(sensor_data.temperature > TEMP_FAN_THRESHOLD)
    {
        fan = 1;  // 开启风扇
    }
    
    // 更新控制状态
    OneNet_SetControl(fan, valve, alarm);*/
}

// ==================== 更新控制输出 ====================
void UpdateControlOutputs(void)
{
   /* uint8_t fan, valve, alarm;
    OneNet_GetControl(&fan, &valve, &alarm);
    
    // 控制LED和蜂鸣器作为输出示例
    // 实际项目中需要根据硬件连接修改
    
    // 风扇 -> LED1
    if(fan) {
        LED1_ON();
    } else {
        LED1_OFF();
    }
    
    // 报警器 -> 蜂鸣器
    if(alarm) {
        Buzzer_ON();
    } else {
        Buzzer_OFF();
    }
    
    // 电磁阀 -> LED2
    if(valve) {
        LED2_ON();
    } else {
        LED2_OFF();
    }*/
}

// ==================== 发送数据到OneNET云平台 ====================
void SendToCloud(void)
{
    static uint8_t send_fail_count = 0;
    
    // 条件：使用定时器中断检查是否需要重连（每30秒检查一次）
    if(GetReconnectFlag())
    {
        sys_status.onenet_connected = 0;  // 标记需要重连
        data_changed = 1;
    }
    
    // 如果OneNET未连接，先尝试连接
    if(!sys_status.onenet_connected)
    {
        if(ConnectToOneNET())
        {
            sys_status.onenet_connected = 1;
            send_fail_count = 0;
        }
        else
        {
            send_fail_count++;
            data_changed = 1;
            
            // 触发后台重连机制
            if(!conn_ctrl.is_connecting)
            {
                conn_ctrl.state = CONN_STATE_WIFI_TEST;
                conn_ctrl.is_connecting = 1;
                conn_ctrl.retry_count = 0;
            }
            return;  // 连接失败，下次再试
        }
    }
    
    // 尝试发送数据（返回0表示成功）
    if(OneNet_SendData() == 0)
    {
        sys_status.packet_sent++;
        send_fail_count = 0;  // 发送成功，重置失败计数
        
        // 更新上次上传的数据
        last_upload_data = sensor_data;
        data_changed = 0;  // 清除变化标志
        last_upload_time = system_tick;  // 记录上传时间
    }
    else
    {
        send_fail_count++;
        
        // 发送失败，标记需要重连
        if(send_fail_count >= 3)  // 3次失败才重连（避免过于敏感）
        {
            sys_status.onenet_connected = 0;  // 标记为未连接，触发重连
            data_changed = 1;
            send_fail_count = 0;
        }
        else
        {
            data_changed = 1;  // 失败后仍然需要上传
        }
    }
    
    // 检查是否有平台下发的数据（命令或属性设置）
    unsigned char *dataPtr = ESP8266_GetIPD(10);
    if(dataPtr != NULL)
    {
        OneNet_RevPro(dataPtr);
    }
}

// ==================== 更新显示 ====================
void UpdateDisplay(void)
{
    char buf[17];
    uint8_t fan, valve, alarm;
    
    OneNet_GetControl(&fan, &valve, &alarm);
    
    // 第1行: 温度和连接状态（右上角显示 C/W/X）
    if(sys_status.wifi_connected && sys_status.onenet_connected) {
        sprintf(buf, "T:%2dC       C", sensor_data.temperature);
    } else if(sys_status.wifi_connected) {
        sprintf(buf, "T:%2dC       W", sensor_data.temperature);
    } else {
        sprintf(buf, "T:%2dC       X", sensor_data.temperature);
    }
    OLED_ShowString(1, 1, buf);
    
    // 第2行: 湿度
    sprintf(buf, "Humi: %2d %%RH", sensor_data.humidity);
    OLED_ShowString(2, 1, buf);
    
    // 第3行: 光照
    sprintf(buf, "L:%5d lx", sensor_data.light);
    OLED_ShowString(3, 1, buf);
    
    // 第4行: 烟雾 + 等级
    sprintf(buf, "S:%5d ", sensor_data.smoke);
    OLED_ShowString(4, 1, buf);
    
    // 显示烟雾等级 L/M/H
    if(sensor_data.smoke <= 1500)
    {
        OLED_ShowString(4, 12, "L ");
    }
    else if(sensor_data.smoke <= 2800)
    {
        OLED_ShowString(4, 12, "M ");
    }
    else
    {
        OLED_ShowString(4, 12, "H ");
    }
}
