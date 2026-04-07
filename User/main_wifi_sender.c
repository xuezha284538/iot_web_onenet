// WiFi发送端 - 超级简单测试版
#include "stm32f10x.h"
#include "Delay.h"
#include "OLED.h"
#include "ESP8266.h"
#include <string.h>
#include <stdio.h>

// ==================== 配置定义 ====================
#define WIFI_SSID       "hello"
#define WIFI_PASS       "12345678"

// ==================== 主函数 ====================
int main(void)
{
    OLED_Init();
    ESP8266_Init();
    
    OLED_Clear();
    OLED_ShowString(1, 1, "=== TEST ===");
    
    // 1. 测试AT指令
    OLED_ShowString(2, 1, "1. AT...");
    if(ESP8266_Test()) {
        OLED_ShowString(3, 1, "AT OK!");
    } else {
        OLED_ShowString(3, 1, "AT FAIL!");
        while(1);
    }
    Delay_ms(1000);
    
    // 2. 设置STA模式
    OLED_Clear();
    OLED_ShowString(1, 1, "=== TEST ===");
    OLED_ShowString(2, 1, "2. MODE...");
    if(ESP8266_SetMode(ESP8266_MODE_STA)) {
        OLED_ShowString(3, 1, "MODE OK!");
    } else {
        OLED_ShowString(3, 1, "MODE FAIL!");
        while(1);
    }
    Delay_ms(1000);
    
    // 3. 连接WiFi
    OLED_Clear();
    OLED_ShowString(1, 1, "=== TEST ===");
    OLED_ShowString(2, 1, "3. WiFi...");
    if(ESP8266_ConnectWiFi(WIFI_SSID, WIFI_PASS)) {
        OLED_ShowString(3, 1, "WiFi OK!");
    } else {
        OLED_ShowString(3, 1, "WiFi FAIL!");
        while(1);
    }
    Delay_ms(1000);
    
    // 4. 显示IP
    OLED_Clear();
    OLED_ShowString(1, 1, "ALL PASS!");
    OLED_ShowString(2, 1, "WiFi OK!");
    
    char ip_buf[16];
    if(ESP8266_GetIP(ip_buf, 1)) {
        OLED_ShowString(3, 1, "IP:");
        OLED_ShowString(4, 1, ip_buf);
    }
    
    while(1) {
        Delay_ms(1000);
    }
}