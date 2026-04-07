#include "stm32f10x.h"
#include "config.h"  // 配置头文件
#include "ESP8266.h"
#include "Delay.h"
#include <string.h>
#include <stdio.h>

#define ESP8266_RX_BUF_SIZE 512
#define ESP8266_BUF_SIZE     256   // 从128扩大到256，防止MQTT包溢出

// 函数前向声明（解决隐式声明警告）
uint8_t ESP8266_WaitResponse(char *response, uint32_t timeout);

// 增强版接收缓冲区
uint8_t esp8266_rx_buf[ESP8266_RX_BUF_SIZE];
uint16_t esp8266_rx_index = 0;

// OneNET兼容缓冲区（扩大到256字节）
unsigned char esp8266_buf[ESP8266_BUF_SIZE];
unsigned short esp8266_cnt = 0, esp8266_cntPre = 0;

// 初始化USART1用于ESP8266（PA9-TX, PA10-RX）
void ESP8266_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;
    
    // 使能时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_USART1, ENABLE);
    
    // TX - PA9
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    
    // RX - PA10
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    
    // USART配置
    USART_InitStructure.USART_BaudRate = ESP8266_BAUD;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(ESP8266_USART, &USART_InitStructure);
    
    // 使能接收中断
    USART_ITConfig(ESP8266_USART, USART_IT_RXNE, ENABLE);
    
    // NVIC配置
    NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 2;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
    
    USART_Cmd(ESP8266_USART, ENABLE);
    
    // 清空接收缓冲区
    esp8266_rx_index = 0;
    memset(esp8266_rx_buf, 0, ESP8266_RX_BUF_SIZE);
    ESP8266_Clear();
}

// ==================== 硬件复位函数 ====================
void ESP8266_Reset(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    
    // 配置GPIOA0作为复位引脚
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    
    // 执行复位
    GPIO_WriteBit(GPIOA, GPIO_Pin_0, Bit_RESET);
    Delay_ms(250);
    GPIO_WriteBit(GPIOA, GPIO_Pin_0, Bit_SET);
    Delay_ms(500);
    
    // 清空缓冲区
    ESP8266_ClearBuffer();
    ESP8266_Clear();
}

// USART1中断处理
void USART1_IRQHandler(void)
{
    if(USART_GetITStatus(ESP8266_USART, USART_IT_RXNE) != RESET)
    {
        uint8_t ch = USART_ReceiveData(ESP8266_USART);
        
        // OneNET兼容缓冲区（P5修复：防止溢出时数据错乱）
        if(esp8266_cnt < ESP8266_BUF_SIZE - 1)
        {
            esp8266_buf[esp8266_cnt++] = ch;
        }
        
        // 增强版缓冲区
        if(esp8266_rx_index < ESP8266_RX_BUF_SIZE - 1)
        {
            esp8266_rx_buf[esp8266_rx_index++] = ch;
            esp8266_rx_buf[esp8266_rx_index] = '\0';
        }
        
        USART_ClearITPendingBit(ESP8266_USART, USART_IT_RXNE);
    }
}

// ==================== 增强版功能函数 ====================

// 发送字符
void ESP8266_SendChar(uint8_t ch)
{
    while(USART_GetFlagStatus(ESP8266_USART, USART_FLAG_TXE) == RESET);
    USART_SendData(ESP8266_USART, ch);
}

// 发送字符串
void ESP8266_SendString(char *str)
{
    while(*str)
    {
        ESP8266_SendChar(*str++);
    }
}

// 发送数据（用于OneNet_DevLink等不需要检查结果的场景）
void ESP8266_SendData(uint8_t *data, uint16_t len)
{
    char cmdBuf[32];
    uint16_t i;
    
    // P4修复：发送前只清空必要的部分，不清全部
    ESP8266_Clear();  // 只清空esp8266_buf（兼容层用）
    
    // 发送 AT+CIPSEND 命令
    sprintf(cmdBuf, "AT+CIPSEND=%d", len);
    
    // 发送命令并等待 >（注意：ESP8266_SendCmd 返回0表示成功）
    if(!ESP8266_SendCmd(cmdBuf, ">"))
    {
        // 收到 >，发送实际数据
        for(i = 0; i < len; i++)
        {
            ESP8266_SendChar(data[i]);
        }
    }
}

// 发送数据并检查发送结果（返回1成功，0失败）
uint8_t ESP8266_SendDataCheck(uint8_t *data, uint16_t len)
{
    char cmdBuf[32];
    uint16_t i;
    uint8_t result = 0;  // 默认失败
    
    // P4修复：发送前只清空esp8266_buf，保留rx_buf用于后续错误检测
    ESP8266_Clear();
    
    // 发送 AT+CIPSEND 命令
    sprintf(cmdBuf, "AT+CIPSEND=%d", len);
    
    // 发送命令并等待 >
    if(!ESP8266_SendCmd(cmdBuf, ">"))
    {
        // 收到 >，发送实际数据
        for(i = 0; i < len; i++)
        {
            ESP8266_SendChar(data[i]);
        }
        
        Delay_ms(200);  // P4修复：增加到200ms，给ESP8266足够时间处理发送和服务器响应
        
        // 检查是否有明确的错误（在rx_buf中查找）
        if(strstr((char*)esp8266_rx_buf, "ERROR") != NULL ||
           strstr((char*)esp8266_rx_buf, "FAIL") != NULL)
        {
            result = 0;  // 明确的错误
        }
        else
        {
            // 没有ERROR/FAIL，认为发送成功（即使没收到SEND OK）
            result = 1;  // 乐观策略：默认成功
        }
    }
    
    return result;
}

// 接收字符（非阻塞）
uint8_t ESP8266_ReceiveChar(uint8_t *ch)
{
    if(esp8266_rx_index > 0)
    {
        *ch = esp8266_rx_buf[0];
        // 移动缓冲区
        memmove(esp8266_rx_buf, esp8266_rx_buf + 1, esp8266_rx_index - 1);
        esp8266_rx_index--;
        return 1;
    }
    return 0;
}

// ==================== 检查接收缓冲区是否有数据 ====================
uint16_t ESP8266_Available(void)
{
    return esp8266_rx_index;
}

// ==================== 从接收缓冲区读取一个字节 ====================
uint8_t ESP8266_ReadByte(void)
{
    uint8_t ch = 0;
    if(esp8266_rx_index > 0)
    {
        ch = esp8266_rx_buf[0];
        // 移动缓冲区数据
        memmove(esp8266_rx_buf, esp8266_rx_buf + 1, esp8266_rx_index - 1);
        esp8266_rx_index--;
        esp8266_rx_buf[esp8266_rx_index] = '\0';
    }
    return ch;
}

// 清空增强版接收缓冲区
void ESP8266_ClearBuffer(void)
{
    esp8266_rx_index = 0;
    memset(esp8266_rx_buf, 0, ESP8266_RX_BUF_SIZE);
}

// 等待响应（必须在 ESP8266_SendDataCheck 之前定义）
uint8_t ESP8266_WaitResponse(char *response, uint32_t timeout)
{
    uint32_t tick = 0;
    while(tick < timeout)
    {
        if(strstr((char*)esp8266_rx_buf, response) != NULL)
        {
            return 1;
        }
        Delay_ms(1);
        tick++;
    }
    return 0;
}

// 发送AT指令（使用增强层）
uint8_t ESP8266_SendAT(char *cmd, char *response, uint32_t timeout)
{
    ESP8266_ClearBuffer();
    ESP8266_Clear();
    ESP8266_SendString(cmd);
    ESP8266_SendString("\r\n");
    
    if(ESP8266_WaitResponse(response, timeout))
    {
        return 1;  // 成功
    }
    return 0;  // 失败
}

// 测试AT指令
uint8_t ESP8266_Test(void)
{
    return ESP8266_SendAT("AT", "OK", 500);
}

// 设置工作模式
uint8_t ESP8266_SetMode(ESP8266_Mode mode)
{
    char cmd[20];
    sprintf(cmd, "AT+CWMODE=%d", mode);
    return ESP8266_SendAT(cmd, "OK", 1000);
}

// 连接WiFi（STA模式）
uint8_t ESP8266_ConnectWiFi(char *ssid, char *password)
{
    char cmd[100];
    ESP8266_SetMode(ESP8266_MODE_STA);
    Delay_ms(100);
    
    // 先断开之前的连接
    ESP8266_SendAT("AT+CWQAP", "OK", 2000);
    Delay_ms(500);
    
    sprintf(cmd, "AT+CWJAP=\"%s\",\"%s\"", ssid, password);
    ESP8266_ClearBuffer();
    ESP8266_SendString(cmd);
    ESP8266_SendString("\r\n");
    
    // 等待连接成功，检查多个可能的响应
    uint32_t tick = 0;
    while(tick < 15000)  // 15秒超时
    {
        // 检查各种成功标志
        if(strstr((char*)esp8266_rx_buf, "WIFI GOT IP") != NULL)
        {
            return 1;  // 获得IP，连接成功
        }
        if(strstr((char*)esp8266_rx_buf, "OK") != NULL && 
           strstr((char*)esp8266_rx_buf, "CWJAP") == NULL)
        {
            // 收到OK但不是命令回显
            Delay_ms(1000);
            // 再检查是否获得IP
            if(strstr((char*)esp8266_rx_buf, "WIFI GOT IP") != NULL)
            {
                return 1;
            }
        }
        if(strstr((char*)esp8266_rx_buf, "FAIL") != NULL)
        {
            return 0;  // 连接失败
        }
        
        Delay_ms(10);
        tick += 10;
    }
    return 0;
}

// 创建AP热点
uint8_t ESP8266_StartAP(char *ssid, char *password, uint8_t channel)
{
    char cmd[100];
    
    // 设置AP模式
    if(!ESP8266_SetMode(ESP8266_MODE_AP))
    {
        return 0;
    }
    Delay_ms(200);
    
    // 配置AP参数
    sprintf(cmd, "AT+CWSAP=\"%s\",\"%s\",%d,3", ssid, password, channel);
    if(!ESP8266_SendAT(cmd, "OK", 5000))
    {
        return 0;
    }
    Delay_ms(200);
    
    // 启用DHCP服务器
    ESP8266_SendAT("AT+CWDHCP=0,1", "OK", 2000);
    Delay_ms(100);
    
    return 1;
}

// 建立TCP连接（客户端）
uint8_t ESP8266_StartTCP(char *ip, uint16_t port)
{
    char cmd[100];
    
    // 设置单连接模式
    ESP8266_SendCmd("AT+CIPMUX=0", "OK");
    Delay_ms(200);
    
    sprintf(cmd, "AT+CIPSTART=\"TCP\",\"%s\",%d", ip, port);
    return !ESP8266_SendCmd(cmd, "CONNECT");  // 返回0表示成功
}

// ==================== 关闭TCP连接 ====================
uint8_t ESP8266_CloseTCP(void)
{
    return !ESP8266_SendCmd("AT+CIPCLOSE", "OK");  // 返回0表示成功
}

// 开启TCP服务器
uint8_t ESP8266_StartServer(uint16_t port)
{
    char cmd[50];
    
    // 设置多连接模式
    ESP8266_SendAT("AT+CIPMUX=1", "OK", 1000);
    Delay_ms(100);
    

    // 开启服务器
    sprintf(cmd, "AT+CIPSERVER=1,%d", port);
    return ESP8266_SendAT(cmd, "OK", 2000);
}

// 发送传感器数据（JSON格式）
void ESP8266_SendSensorData(uint8_t temp, uint8_t humi, uint16_t light, uint16_t smoke)
{
    char json_buf[100];
    char cmd[20];
    uint16_t len;
    
    // 构造JSON数据
    sprintf(json_buf, "{\"T\":%d,\"H\":%d,\"L\":%d,\"S\":%d}\r\n", 
            temp, humi, light, smoke);

    len = strlen(json_buf);
    
    // 发送数据长度
    sprintf(cmd, "AT+CIPSEND=0,%d", len);
    ESP8266_SendAT(cmd, ">", 1000);
    Delay_ms(10);
    
    // 发送数据
    ESP8266_SendString(json_buf);
}

// 发送传感器数据（简单格式）
void ESP8266_SendSensorDataSimple(uint8_t temp, uint8_t humi, uint16_t light, uint16_t smoke)
{
    char buf[50];
    
    // 格式: TEMP,HUMI,LIGHT,SMOKE\r\n
    sprintf(buf, "%d,%d,%d,%d\r\n", temp, humi, light, smoke);
    
    ESP8266_SendString(buf);
}

// 获取IP地址
// is_sta: 1=STA模式IP, 0=AP模式IP
uint8_t ESP8266_GetIP(char *ip_buf, uint8_t is_sta)
{
    char *response = is_sta ? "STAIP" : "APIP";
    
    ESP8266_ClearBuffer();
    ESP8266_SendString("AT+CIFSR");
    ESP8266_SendString("\r\n");
    
    uint32_t tick = 0;
    while(tick < 3000)
    {
        // 查找IP地址
        char *ip_start = strstr((char*)esp8266_rx_buf, "\"");
        if(ip_start != NULL)
        {
            ip_start++;  // 跳过第一个引号
            char *ip_end = strstr(ip_start, "\"");
            if(ip_end != NULL)
            {
                int len = ip_end - ip_start;
                if(len > 7 && len < 16)  // IP地址长度检查
                {
                    strncpy(ip_buf, ip_start, len);
                    ip_buf[len] = '\0';
                    return 1;
                }
            }
        }
        
        Delay_ms(10);
        tick += 10;
    }
    return 0;
}

// ==================== OneNET兼容功能 ====================

// P7修复：Clear时同时重置cntPre
void ESP8266_Clear(void)
{
    memset(esp8266_buf, 0, sizeof(esp8266_buf));
    esp8266_cnt = 0;
    esp8266_cntPre = 0;  // P7修复：重置cntPre，防止WaitRecive误判
}

_Bool ESP8266_WaitRecive(void)
{
    if(esp8266_cnt == 0)
        return REV_WAIT;

    if(esp8266_cnt == esp8266_cntPre)
    {
        esp8266_cnt = 0;
        return REV_OK;
    }

    esp8266_cntPre = esp8266_cnt;
    return REV_WAIT;
}

_Bool ESP8266_SendCmd(char *cmd, char *res)
{
    unsigned char timeOut = 200;

    ESP8266_ClearBuffer();
    ESP8266_Clear();  // P7修复：现在Clear会重置cntPre
    ESP8266_SendString(cmd);
    ESP8266_SendString("\r\n");

    while(timeOut--)
    {
        if(ESP8266_WaitRecive() == REV_OK)
        {
            if(strstr((const char *)esp8266_buf, res) != NULL)
            {
                ESP8266_Clear();
                return 0;  // 成功
            }
        }
        Delay_ms(10);
    }
    return 1;  // 失败
}

unsigned char *ESP8266_GetIPD(unsigned short timeOut)
{
    char *ptrIPD = NULL;

    do
    {
        if(ESP8266_WaitRecive() == REV_OK)
        {
            ptrIPD = strstr((char *)esp8266_buf, "IPD,");
            if(ptrIPD != NULL)
            {
                ptrIPD = strchr(ptrIPD, ':');
                if(ptrIPD != NULL)
                {
                    ptrIPD++;
                    return (unsigned char *)(ptrIPD);
                }
                else
                    return NULL;
            }
        }
        Delay_ms(5);
    } while(timeOut--);

    return NULL;
}
