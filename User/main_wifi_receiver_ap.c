// WiFi接收端 - 创建热点(AP模式)，启动TCP服务器接收数据
#include "stm32f10x.h"
#include "Delay.h"
#include "OLED.h"
#include "ESP8266.h"
#include "LED.h"
#include <string.h>
#include <stdio.h>

// 外部变量声明（来自ESP8266.c）
extern uint8_t esp8266_rx_buf[];
extern uint16_t esp8266_rx_index;
extern void ESP8266_ClearBuffer(void);

// ==================== 配置定义 ====================
// WiFi配置 - 创建热点
#define WIFI_SSID       "STM32_Server"
#define WIFI_PASS       "12345678"
#define WIFI_CHANNEL    6

// TCP服务器配置
#define LOCAL_PORT      8080

// 系统参数
#define DATA_TIMEOUT_MS     5000    // 数据接收超时时间(ms)
#define DISPLAY_REFRESH_MS  200     // 显示刷新间隔(ms)
#define LED_BLINK_INTERVAL  500     // LED闪烁间隔(ms)
#define ESP8266_RX_BUF_SIZE 512     // ESP8266接收缓冲区大小

// ==================== 数据结构 ====================
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
    uint8_t wifi_ready;         // WiFi热点创建状态
    uint8_t tcp_started;        // TCP服务器启动状态
    uint8_t client_connected;   // 客户端连接状态
    uint8_t data_receiving;     // 是否正在接收数据
    uint32_t last_data_time;    // 最后接收数据时间
    uint32_t packet_count;      // 接收数据包计数
    uint32_t error_count;       // 错误计数
} SystemStatus_t;

// ==================== 全局变量 ====================
SensorData_t sensor_data = {0};
SystemStatus_t sys_status = {0};

// 时间戳
volatile uint32_t system_tick = 0;

// 接收数据解析缓冲区
char rx_parse_buf[128];

// ==================== 函数声明 ====================
void System_Init(void);
void ParseReceivedData(char *data);
uint8_t ValidateSensorData(SensorData_t *data);
void UpdateDisplay(void);
void UpdateLED(void);
void CheckConnection(void);
uint8_t SetupWiFiAP(void);
uint8_t StartTCPServer(void);
void ShowStartupScreen(void);
void ProcessReceivedData(void);
void MonitorESP8266Status(void);

// ==================== 主函数 ====================
int main(void)
{
    System_Init();
    ShowStartupScreen();
    Delay_ms(1000);
    
    // 设置WiFi热点
    OLED_Clear();
    OLED_ShowString(1, 1, "WiFi AP Init...");
    
    if(!SetupWiFiAP()) {
        OLED_ShowString(2, 1, "AP Failed!");
        Delay_ms(2000);
    } else {
        OLED_ShowString(2, 1, "AP Ready!");
        OLED_ShowString(3, 1, WIFI_SSID);
        Delay_ms(1000);
    }
    
    OLED_ShowString(4, 1, "Start Server...");
    
    if(!StartTCPServer()) {
        OLED_ShowString(4, 1, "Server Fail!");
        Delay_ms(2000);
    } else {
        OLED_ShowString(4, 1, "Server Ready!");
        Delay_ms(500);
    }
    
    OLED_Clear();
    
    // 主循环
    while(1)
    {
        // 1. 处理接收到的数据
        ProcessReceivedData();
        
        // 2. 检查连接状态
        CheckConnection();
        
        // 3. 监控ESP8266状态
        static uint32_t last_esp8266_check = 0;
        if(system_tick - last_esp8266_check >= 30000)  // 每30秒检查一次
        {
            last_esp8266_check = system_tick;
            MonitorESP8266Status();
        }
        
        // 4. 更新显示
        static uint32_t last_display_update = 0;
        if(system_tick - last_display_update >= DISPLAY_REFRESH_MS)
        {
            last_display_update = system_tick;
            UpdateDisplay();
        }
        
        // 5. 更新LED状态
        UpdateLED();
        
        Delay_ms(10);
        system_tick += 10;
    }
}

// ==================== 系统初始化 ====================
void System_Init(void)
{
    OLED_Init();
    LED_Init();
    ESP8266_Init();
    
    memset(&sensor_data, 0, sizeof(SensorData_t));
    memset(&sys_status, 0, sizeof(SystemStatus_t));
    
    LED1_OFF();
    LED2_OFF();
}

// ==================== 启动画面 ====================
void ShowStartupScreen(void)
{
    OLED_Clear();
    OLED_ShowString(1, 1, "WiFi Receiver");
    OLED_ShowString(2, 1, "AP Mode");
    OLED_ShowString(3, 1, "Waiting for");
    OLED_ShowString(4, 1, "connection...");
}

// ==================== 处理接收数据 ====================
void ProcessReceivedData(void)
{
    if(esp8266_rx_index > 0)
    {
        // 查找完整的数据包（以\n或\r结尾）
        char *end = strstr((char*)esp8266_rx_buf, "\n");
        if(end == NULL) end = strstr((char*)esp8266_rx_buf, "\r");
        
        if(end != NULL)
        {
            int len = end - (char*)esp8266_rx_buf;
            if(len > 0 && len < sizeof(rx_parse_buf))
            {
                memcpy(rx_parse_buf, esp8266_rx_buf, len);
                rx_parse_buf[len] = '\0';
                
                // 检查是否是IPD数据 (+IPD,x,y:data)
                char *data_start = strstr(rx_parse_buf, "+IPD,");
                if(data_start != NULL)
                {
                    char *colon = strchr(data_start, ':');
                    if(colon != NULL)
                    {
                        ParseReceivedData(colon + 1);
                        sys_status.client_connected = 1;
                        sys_status.data_receiving = 1;
                        sys_status.last_data_time = system_tick;
                        sys_status.packet_count++;
                    }
                }
                else if(strstr(rx_parse_buf, "CONNECT"))
                {
                    sys_status.client_connected = 1;
                }
                else if(strstr(rx_parse_buf, "CLOSED"))
                {
                    sys_status.client_connected = 0;
                }
            }
            
            // 移动缓冲区
            int remaining = esp8266_rx_index - (end - (char*)esp8266_rx_buf) - 1;
            if(remaining > 0)
            {
                memmove(esp8266_rx_buf, end + 1, remaining);
                esp8266_rx_index = remaining;
            }
            else
            {
                ESP8266_ClearBuffer();
            }
        }
        else if(esp8266_rx_index >= ESP8266_RX_BUF_SIZE - 10)
        {
            ESP8266_ClearBuffer();
            sys_status.error_count++;
        }
    }
}

// ==================== 解析接收数据 ====================
void ParseReceivedData(char *data)
{
    int t, h, l, s;
    uint8_t parse_ok = 0;
    
    // 尝试解析JSON格式: {"T":25,"H":60,"L":500,"S":2000}
    if(sscanf(data, "{\"T\":%d,\"H\":%d,\"L\":%d,\"S\":%d}", &t, &h, &l, &s) == 4)
    {
        parse_ok = 1;
    }
    // 尝试解析简单格式: 25,60,500,2000
    else if(sscanf(data, "%d,%d,%d,%d", &t, &h, &l, &s) == 4)
    {
        parse_ok = 1;
    }
    
    if(parse_ok)
    {
        SensorData_t new_data;
        new_data.temperature = (uint8_t)t;
        new_data.humidity = (uint8_t)h;
        new_data.light = (uint16_t)l;
        new_data.smoke = (uint16_t)s;
        
        if(ValidateSensorData(&new_data))
        {
            memcpy(&sensor_data, &new_data, sizeof(SensorData_t));
            sensor_data.valid = 1;
        }
        else
        {
            sys_status.error_count++;
        }
    }
}

// ==================== 数据有效性验证 ====================
uint8_t ValidateSensorData(SensorData_t *data)
{
    if(data->humidity > 100) return 0;
    return 1;
}

// ==================== 设置WiFi热点 ====================
uint8_t SetupWiFiAP(void)
{
    // 测试AT
    OLED_ShowString(2, 1, "Test AT...");
    if(!ESP8266_Test())
    {
        OLED_ShowString(2, 1, "AT Fail!    ");
        return 0;
    }
    OLED_ShowString(2, 1, "AT OK       ");
    Delay_ms(200);
    
    // 设置AP模式
    OLED_ShowString(2, 1, "Set AP...   ");
    if(!ESP8266_SetMode(ESP8266_MODE_AP))
    {
        OLED_ShowString(2, 1, "AP Fail!    ");
        return 0;
    }
    OLED_ShowString(2, 1, "AP OK       ");
    Delay_ms(200);
    
    // 创建热点
    OLED_ShowString(2, 1, "Start AP... ");
    if(!ESP8266_StartAP(WIFI_SSID, WIFI_PASS, WIFI_CHANNEL))
    {
        OLED_ShowString(2, 1, "Start Fail! ");
        return 0;
    }
    OLED_ShowString(2, 1, "AP Ready    ");
    sys_status.wifi_ready = 1;
    Delay_ms(500);
    
    return 1;
}

// ==================== 启动TCP服务器 ====================
uint8_t StartTCPServer(void)
{
    OLED_ShowString(2, 1, "TCP Server..");
    
    if(!ESP8266_StartServer(LOCAL_PORT))
    {
        OLED_ShowString(2, 1, "TCP Fail!   ");
        return 0;
    }
    
    OLED_ShowString(2, 1, "TCP OK      ");
    sys_status.tcp_started = 1;
    Delay_ms(500);
    
    return 1;
}

// ==================== 检查连接状态 ====================
void CheckConnection(void)
{
    // 检查数据接收超时
    if(sys_status.data_receiving && 
       (system_tick - sys_status.last_data_time > DATA_TIMEOUT_MS))
    {
        sys_status.data_receiving = 0;
        sys_status.client_connected = 0;
        sensor_data.valid = 0;
        sys_status.error_count++;
    }
    
    // 如果客户端断开连接，重新启动TCP服务器
    if(sys_status.wifi_ready && sys_status.tcp_started && !sys_status.client_connected)
    {
        static uint32_t last_reconnect_attempt = 0;
        static uint8_t reconnect_count = 0;
        
        // 每隔5秒尝试重新启动服务器
        if(system_tick - last_reconnect_attempt >= 5000)
        {
            last_reconnect_attempt = system_tick;
            reconnect_count++;
            
            OLED_ShowString(4, 1, "Reconnect...");
            
            // 先关闭服务器
            ESP8266_SendAT("AT+CIPSERVER=0", "OK", 2000);
            Delay_ms(500);
            
            // 重新启动服务器
            if(ESP8266_StartServer(LOCAL_PORT))
            {
                OLED_ShowString(4, 1, "Server Ready!");
                sys_status.tcp_started = 1;
                reconnect_count = 0;
            }
            else
            {
                OLED_ShowString(4, 1, "Server Fail! ");
                sys_status.error_count++;
                
                // 如果多次失败，重新初始化WiFi
                if(reconnect_count >= 3)
                {
                    reconnect_count = 0;
                    OLED_ShowString(4, 1, "Reinit WiFi..");
                    SetupWiFiAP();
                }
            }
        }
    }
}

// ==================== 更新LED状态 ====================
void UpdateLED(void)
{
    static uint32_t last_led_toggle = 0;
    static uint8_t led_state = 0;
    
    if(system_tick - last_led_toggle >= LED_BLINK_INTERVAL)
    {
        last_led_toggle = system_tick;
        led_state = !led_state;
        
        if(sys_status.wifi_ready && sys_status.client_connected)
        {
            LED1_ON();
            if(led_state) LED2_ON();
            else LED2_OFF();
        }
        else if(sys_status.wifi_ready)
        {
            if(led_state) LED1_ON();
            else LED1_OFF();
            LED2_OFF();
        }
        else
        {
            LED2_ON();
            if(led_state) LED1_ON();
            else LED1_OFF();
        }
    }
}

// ==================== 更新显示 ====================
void UpdateDisplay(void)
{
    char buf[17];
    
    // 第1行: 温度和连接状态
    sprintf(buf, "Temp:%2dC  %s", 
            sensor_data.temperature,
            sys_status.client_connected ? "ON " : "OFF");
    OLED_ShowString(1, 1, buf);
    
    // 第2行: 湿度
    sprintf(buf, "Humi: %2d %%RH", sensor_data.humidity);
    OLED_ShowString(2, 1, buf);
    
    // 第3行: 光照
    sprintf(buf, "Light:%5d lx", sensor_data.light);
    OLED_ShowString(3, 1, buf);
    
    // 第4行: 烟雾
    sprintf(buf, "Smoke:%5d", sensor_data.smoke);
    OLED_ShowString(4, 1, buf);
}

// ==================== 监控ESP8266状态 ====================
void MonitorESP8266Status(void)
{
    // 测试ESP8266是否正常响应
    if(!ESP8266_Test())
    {
        OLED_ShowString(4, 1, "ESP8266 Error!");
        sys_status.error_count++;
        
        // 尝试重新初始化ESP8266
        ESP8266_Init();
        Delay_ms(1000);
        
        // 重新设置WiFi和服务器
        if(ESP8266_Test())
        {
            OLED_ShowString(4, 1, "ESP8266 Reset");
            SetupWiFiAP();
            StartTCPServer();
        }
    }
    
    // 检查WiFi状态
    if(sys_status.wifi_ready)
    {
        char ip_buf[20];
        if(ESP8266_GetIP(ip_buf, 0))  // 获取AP模式IP
        {
            // IP地址获取成功，WiFi正常
        }
        else
        {
            OLED_ShowString(4, 1, "WiFi Error!  ");
            sys_status.wifi_ready = 0;
            sys_status.error_count++;
        }
    }
}
