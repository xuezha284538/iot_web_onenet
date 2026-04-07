#ifndef __ESP8266_H
#define __ESP8266_H

#include "stm32f10x.h"

#define ESP8266_USART   USART1
#define ESP8266_BAUD    115200

#define REV_OK		0	// 接收完成标志
#define REV_WAIT	1	// 接收未完成标志

// 接收缓冲区（供调试使用）
extern uint8_t esp8266_rx_buf[];
extern uint16_t esp8266_rx_index;

// ESP8266工作模式
typedef enum {
    ESP8266_MODE_STA = 1,       // 站点模式
    ESP8266_MODE_AP = 2,        // AP模式
    ESP8266_MODE_AP_STA = 3     // AP+STA模式
} ESP8266_Mode;

void ESP8266_Init(void);
void ESP8266_SendChar(uint8_t ch);
void ESP8266_SendString(char *str);
void ESP8266_SendData(uint8_t *data, uint16_t len);
uint8_t ESP8266_SendDataCheck(uint8_t *data, uint16_t len);  // 发送数据并检查结果
uint8_t ESP8266_ReceiveChar(uint8_t *ch);

// 缓冲区操作函数
uint16_t ESP8266_Available(void);
uint8_t ESP8266_ReadByte(void);
void ESP8266_ClearBuffer(void);

// 硬件复位
void ESP8266_Reset(void);

// AT指令函数
uint8_t ESP8266_Test(void);
uint8_t ESP8266_SetMode(ESP8266_Mode mode);
uint8_t ESP8266_ConnectWiFi(char *ssid, char *password);
uint8_t ESP8266_StartAP(char *ssid, char *password, uint8_t channel);
uint8_t ESP8266_StartTCP(char *ip, uint16_t port);
uint8_t ESP8266_CloseTCP(void);
uint8_t ESP8266_StartServer(uint16_t port);
uint8_t ESP8266_SendAT(char *cmd, char *response, uint32_t timeout);

// 发送传感器数据
void ESP8266_SendSensorData(uint8_t temp, uint8_t humi, uint16_t light, uint16_t smoke);
void ESP8266_SendSensorDataSimple(uint8_t temp, uint8_t humi, uint16_t light, uint16_t smoke);

// 获取IP地址
uint8_t ESP8266_GetIP(char *ip_buf, uint8_t is_sta);

// ==================== OneNET兼容函数 ====================

void ESP8266_Clear(void);
_Bool ESP8266_WaitRecive(void);
_Bool ESP8266_SendCmd(char *cmd, char *res);
unsigned char *ESP8266_GetIPD(unsigned short timeOut);

#endif
