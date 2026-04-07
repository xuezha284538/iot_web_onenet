// WiFi发送端 - 连接热点(STA模式)，作为TCP客户端发送数据
#include "stm32f10x.h"
#include "Delay.h"
#include "OLED.h"
#include "ESP8266.h"
#include "DHT11.h"
#include "BH1750.h"
#include "MQ2.h"
#include "LED.h"
#include <string.h>
#include <stdio.h>

// ==================== 配置定义 ====================
// WiFi配置 - 连接到接收端的热点
#define WIFI_SSID       "STM32_Server"
#define WIFI_PASS       "12345678"

// TCP服务器配置（ESP8266 AP默认IP是192.168.4.1）
#define SERVER_IP       "192.168.4.1"
#define SERVER_PORT     8080

// 系统参数
#define SENSOR_READ_INTERVAL    100     // 传感器读取间隔(ms)
#define DATA_SEND_INTERVAL      500     // 数据发送间隔(ms)
#define DISPLAY_REFRESH_MS      200     // 显示刷新间隔(ms)
#define LED_BLINK_INTERVAL      500     // LED闪烁间隔(ms)
#define CONNECTION_RETRY_MS     5000    // 连接重试间隔(ms)

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
    uint8_t wifi_connected;     // WiFi连接状态
    uint8_t tcp_connected;      // TCP连接状态
    uint32_t packet_sent;       // 发送数据包计数
    uint32_t error_count;       // 错误计数
} SystemStatus_t;

// ==================== 全局变量 ====================
SensorData_t sensor_data = {0};
SystemStatus_t sys_status = {0};

// 时间戳
volatile uint32_t system_tick = 0;

// 发送数据缓冲区
char tx_buf[128];

// ==================== 函数声明 ====================
void System_Init(void);
void ReadSensors(void);
void SendSensorData(void);
void UpdateDisplay(void);
void UpdateLED(void);
uint8_t ConnectWiFi(void);
uint8_t ConnectToServer(void);
void ShowStartupScreen(void);
void FormatSensorData(char *buf, SensorData_t *data);

// ==================== 主函数 ====================
int main(void)
{
    System_Init();
    ShowStartupScreen();
    Delay_ms(1000);
    
    OLED_Clear();
    
    // 主循环变量
    uint32_t last_sensor_read = 0;
    uint32_t last_data_send = 0;
    uint32_t last_display_update = 0;
    uint32_t last_connection_check = 0;
    
    // 主循环
    while(1)
    {
        // 1. 检查并维护连接
        if(system_tick - last_connection_check >= CONNECTION_RETRY_MS)
        {
            last_connection_check = system_tick;
            
            // 如果未连接WiFi，尝试连接
            if(!sys_status.wifi_connected)
            {
                OLED_ShowString(1, 1, "Connect WiFi... ");
                OLED_ShowString(2, 1, WIFI_SSID);
                
                if(ConnectWiFi())
                {
                    sys_status.wifi_connected = 1;
                    OLED_ShowString(3, 1, "WiFi OK!        ");
                }
                else
                {
                    OLED_ShowString(3, 1, "WiFi Fail!      ");
                }
                Delay_ms(1000);
            }
            
            // 如果WiFi已连接但TCP未连接，尝试连接TCP
            if(sys_status.wifi_connected && !sys_status.tcp_connected)
            {
                OLED_ShowString(3, 1, "Connect TCP...  ");
                OLED_ShowString(4, 1, SERVER_IP);
                
                if(ConnectToServer())
                {
                    sys_status.tcp_connected = 1;
                    OLED_Clear();
                }
                else
                {
                    OLED_ShowString(3, 1, "TCP Fail!       ");
                    OLED_ShowString(4, 1, "Retry later...  ");
                }
                Delay_ms(1000);
            }
        }
        
        // 2. 读取传感器数据
        if(system_tick - last_sensor_read >= SENSOR_READ_INTERVAL)
        {
            last_sensor_read = system_tick;
            ReadSensors();
        }
        
        // 3. 发送数据到服务器
        if(system_tick - last_data_send >= DATA_SEND_INTERVAL)
        {
            last_data_send = system_tick;
            if(sys_status.tcp_connected)
            {
                SendSensorData();
            }
        }
        
        // 4. 更新显示
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
    BH1750_Init();
    MQ2_Init();
    ESP8266_Init();
    LED_Init();
    
    memset(&sensor_data, 0, sizeof(SensorData_t));
    memset(&sys_status, 0, sizeof(SystemStatus_t));
    
    system_tick = 0;
}

// ==================== 启动画面 ====================
void ShowStartupScreen(void)
{
    OLED_Clear();
    OLED_ShowString(1, 1, "================");
    OLED_ShowString(2, 1, "  WiFi Sender   ");
    OLED_ShowString(3, 1, "  STA Client    ");
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

// ==================== 连接TCP服务器 ====================
uint8_t ConnectToServer(void)
{
    return ESP8266_StartTCP(SERVER_IP, SERVER_PORT);
}

// ==================== 读取传感器数据 ====================
void ReadSensors(void)
{
    static uint8_t dht_timer = 0;
    static uint16_t light_accum = 0;
    static uint16_t smoke_accum = 0;
    static uint8_t sample_count = 0;
    
    // 读取DHT11 (每1000ms)
    if(++dht_timer >= 10)
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
    }
}

// ==================== 格式化传感器数据 ====================
void FormatSensorData(char *buf, SensorData_t *data)
{
    // JSON格式: {"T":25,"H":60,"L":500,"S":2000}
    sprintf(buf, "{\"T\":%d,\"H\":%d,\"L\":%d,\"S\":%d}\r\n",
            data->temperature,
            data->humidity,
            data->light,
            data->smoke);
}

// ==================== 发送传感器数据 ====================
void SendSensorData(void)
{
    if(!sensor_data.valid) {
        return;
    }
    
    // 格式化数据
    FormatSensorData(tx_buf, &sensor_data);
    
    // 发送数据长度指令
    char cmd[32];
    uint16_t len = strlen(tx_buf);
    sprintf(cmd, "AT+CIPSEND=%d", len);
    
    if(ESP8266_SendAT(cmd, ">", 1000))
    {
        Delay_ms(10);
        // 发送实际数据
        ESP8266_SendString(tx_buf);
        sys_status.packet_sent++;
    }
    else
    {
        // 发送失败，标记TCP断开，下次重连
        sys_status.tcp_connected = 0;
        sys_status.error_count++;
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
        
        if(sys_status.wifi_connected && sys_status.tcp_connected)
        {
            // WiFi和TCP都连接，LED1常亮，LED2闪烁
            LED1_ON();
            if(led_state) LED2_ON();
            else LED2_OFF();
        }
        else if(sys_status.wifi_connected)
        {
            // 只有WiFi连接，LED1闪烁
            if(led_state) LED1_ON();
            else LED1_OFF();
            LED2_OFF();
        }
        else
        {
            // 都未连接，LED2常亮，LED1闪烁
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
    char *status_str;
    if(sys_status.wifi_connected && sys_status.tcp_connected) {
        status_str = "C";  // Connected
    } else if(sys_status.wifi_connected) {
        status_str = "W";  // WiFi only
    } else {
        status_str = "X";  // Disconnected
    }
    
    sprintf(buf, "T:%2dC S:%lu %s", 
            sensor_data.temperature,
            sys_status.packet_sent % 100,
            status_str);
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
