#ifndef _ONENET_H_
#define _ONENET_H_
#include "stm32f10x.h"


//#define MQTT_PUBLISH_ID			30

// 全局传感器数据变量声明
extern uint8_t sensor_temp;
extern uint8_t sensor_humi;
extern uint16_t sensor_light;
extern uint16_t sensor_smoke;

// 全局控制设备状态变量声明
extern uint8_t control_fan;
extern uint8_t control_valve;
extern uint8_t control_alarm;

_Bool OneNet_DevLink(void);

uint8_t OneNet_SendData(void);

void OneNet_RevPro(unsigned char *cmd);

unsigned char MqttOnenet_Savedata(char *t_payload);

// 控制设备相关函数
void OneNet_SetControl(uint8_t fan, uint8_t valve, uint8_t alarm);
void OneNet_GetControl(uint8_t *fan, uint8_t *valve, uint8_t *alarm);

#endif
