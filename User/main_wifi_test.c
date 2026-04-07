// WiFi连接热点测试程序
#include "stm32f10x.h"
#include "Delay.h"
#include "OLED.h"
#include "ESP8266.h"

// WiFi配置
#define WIFI_SSID   "hello"
#define WIFI_PASS   "12345678"

// 显示ESP8266返回的数据（用于调试）
void ShowESP8266Response(void)
{
    extern uint8_t esp8266_rx_buf[];
    extern uint16_t esp8266_rx_index;
    
    OLED_ShowString(3, 1, "                ");
    OLED_ShowString(4, 1, "                ");
    
    // 显示接收到的前16个字符
    char buf[17];
    uint16_t i;
    for(i = 0; i < 16 && i < esp8266_rx_index; i++)
    {
        buf[i] = esp8266_rx_buf[i];
        if(buf[i] < 32 || buf[i] > 126) buf[i] = '.';  // 不可显示字符用点代替
    }
    buf[i] = '\0';
    
    OLED_ShowString(3, 1, buf);
    
    // 显示长度
    char len_buf[8];
    sprintf(len_buf, "Len:%d", esp8266_rx_index);
    OLED_ShowString(4, 1, len_buf);
}

int main(void)
{
    OLED_Init();
    ESP8266_Init();
    OLED_Clear();
    
    OLED_ShowString(1, 1, "WiFi Test");
    OLED_ShowString(2, 1, "Init...");
    
    Delay_ms(1000);
    
    // 步骤1: 测试AT指令
    OLED_ShowString(2, 1, "Step1: AT Test ");
    if(ESP8266_Test())
    {
        OLED_ShowString(2, 1, "Step1: AT OK    ");
    }
    else
    {
        OLED_ShowString(2, 1, "Step1: AT FAIL  ");
        ShowESP8266Response();
    }
    Delay_ms(1000);
    
    // 步骤2: 设置STA模式
    OLED_ShowString(2, 1, "Step2: STA Mode ");
    if(ESP8266_SetMode(ESP8266_MODE_STA))
    {
        OLED_ShowString(2, 1, "Step2: STA OK   ");
    }
    else
    {
        OLED_ShowString(2, 1, "Step2: STA FAIL ");
        ShowESP8266Response();
    }
    Delay_ms(1000);
    
    // 步骤3: 连接WiFi
    OLED_ShowString(2, 1, "Step3: Connect  ");
    OLED_ShowString(3, 1, WIFI_SSID);
    
    if(ESP8266_ConnectWiFi(WIFI_SSID, WIFI_PASS))
    {
        OLED_ShowString(2, 1, "Step3: WiFi OK  ");
        OLED_ShowString(4, 1, "Connected!      ");
    }
    else
    {
        OLED_ShowString(2, 1, "Step3: WiFi FAIL");
        OLED_ShowString(4, 1, "Failed!         ");
        ShowESP8266Response();
    }
    
    // 主循环 - 显示状态
    while(1)
    {
        Delay_ms(1000);
        
        // 可以在这里添加更多测试
        // 比如查询IP地址: AT+CIFSR
    }
}
