/**
	**********************************************************************
	**********************************************************************
	**********************************************************************
	*	文件名: 	onenet.c
	*	作者: 		zh
	*	日期: 		
	*	版本号: 		V1.1
	*	说明: 		与onenet平台进行数据交互
	*	修改记录:	
	*	
	**********************************************************************
	**********************************************************************
	**********************************************************************
**/

//芯片头文件
#include "stm32f10x.h"

//配置文件
#include "config.h"  // 配置头文件

//外设驱动
#include "ESP8266.h"
#include "OLED.h"

//协议文件
#include "onenet.h"
#include "MqttKit.h"

//硬件驱动
#include "UART.h"
#include "Delay.h"
//#include "dht11.h"

//C库
#include <string.h>
#include <stdio.h>

// OneNET配置已移至config.h
#define PROID		ONENET_PRODUCT_ID  // 产品ID
#define AUTH_INFO	ONENET_AUTH_INFO   // 鉴权信息
#define DEVID		ONENET_DEVICE_NAME // 设备名称

// OneNET服务器配置已移至config.h
// #define ONENET_SERVER_IP 	"mqtts.heclouds.com"
// #define ONENET_SERVER_PORT 	1883

// 全局传感器数据变量定义
uint8_t sensor_temp = 0;
uint8_t sensor_humi = 0;
uint16_t sensor_light = 0;
uint16_t sensor_smoke = 0;

// 全局控制设备状态变量定义
uint8_t control_fan = 0;
uint8_t control_valve = 0;
uint8_t control_alarm = 0;

extern unsigned char esp8266_buf[128];

//==========================================================
//	函数名称:	OneNet_DevLink
//
//	函数功能:	与onenet建立连接
//
//	入口参数:	无
//
//	返回参数:	1-成功	0-失败
//
//	说明:		与onenet平台建立连接
//==========================================================
_Bool OneNet_DevLink(void)
{
	MQTT_PACKET_STRUCTURE mqttPacket = {NULL, 0, 0, 0};					//协议包

	unsigned char *dataPtr;
	
	_Bool status = 1;
	
	// 1. 先关闭可能存在的旧连接
	ESP8266_CloseTCP();
	Delay_ms(200);
	
	// 2. 建立 TCP 连接到 OneNET 服务器
	if(!ESP8266_StartTCP(ONENET_SERVER_IP, ONENET_SERVER_PORT))
	{
		return 1;  // TCP 连接失败
	}
	Delay_ms(200);
	
	if(MQTT_PacketConnect(PROID, AUTH_INFO, DEVID, 256, 1, MQTT_QOS_LEVEL0, NULL, NULL, 0, &mqttPacket) == 0)  //修改clean_session=1
	{
		ESP8266_SendData(mqttPacket._data, mqttPacket._len);	//上传平台
		
		dataPtr = ESP8266_GetIPD(1000);	//等待平台响应（增加超时时间到10秒）
		if(dataPtr != NULL)
		{
			if(MQTT_UnPacketRecv(dataPtr) == MQTT_PKT_CONNACK)
			{
				switch(MQTT_UnPacketConnectAck(dataPtr))
				{
					case 0:status = 0; break;
					
					case 1:break;
					case 2:break;
					case 3:break;
					case 4:break;
					case 5:break;
					
					default:break;
				}
			}
		}
		
		MQTT_DeleteBuffer(&mqttPacket);		//删除
	}
	
	return status;
	
}


//生成ONENET需要上传JSON数据，把获得温度湿度转换为JSON数据格式
unsigned char MqttOnenet_Savedata(char *t_payload)
{
	char json[]="{\"id\":\"123\",\"version\":\"1.0\",\"msgId\":\"K3GGK1Sj7Q\",\"params\":{\"temperature\":{\"value\":%d},\"humidity\":{\"value\":%d},\"light\":{\"value\":%d},\"smoke\":{\"value\":%d}}}";
    char t_json[256];
    unsigned short json_len;
	sprintf(t_json, json, sensor_temp, sensor_humi, sensor_light, sensor_smoke);
    json_len = strlen(t_json)/sizeof(char);
  	memcpy(t_payload, t_json, json_len);
	
    return json_len;	
}

//==========================================================
//	函数名称:	OneNet_SendData
//
//	函数功能:	上传数据到平台
//
//	入口参数:	type:上传数据的格式
//
//	返回参数:	0-成功 1-失败
//
//	说明:		
//==========================================================
uint8_t OneNet_SendData(void)
{
	
	MQTT_PACKET_STRUCTURE mqttPacket = {NULL, 0, 0, 0};		//协议包
	
	char buf[200];
	short body_len = 0, i = 0;
	uint8_t result = 1;  // 默认失败
	
	memset(buf, 0, sizeof(buf));    //清空buff
	body_len=MqttOnenet_Savedata(buf);	
	
	if(body_len)
	{
		
		if(MQTT_PacketSaveData(DEVID, body_len, NULL, 5, &mqttPacket) == 0)							//打包
		{
			
			for(; i < body_len; i++)
				mqttPacket._data[mqttPacket._len++] = buf[i];
			
			if(ESP8266_SendDataCheck(mqttPacket._data, mqttPacket._len))  //上传数据到平台并检查结果
			{
				result = 0;  // 成功
			}
			
			MQTT_DeleteBuffer(&mqttPacket);															//删除
		}
	}
	
	return result;
}

//==========================================================
//	函数名称:	OneNet_RevPro
//
//	函数功能:	平台下发数据处理
//
//	入口参数:	dataPtr:平台返回的数据
//
//	返回参数:	无
//
//	说明:		
//==========================================================
void OneNet_RevPro(unsigned char *cmd)
{
	
	MQTT_PACKET_STRUCTURE mqttPacket = {NULL, 0, 0, 0};								//协议包
	
	char *req_payload = NULL;
	char *cmdid_topic = NULL;
	
	unsigned short req_len = 0;
	
	unsigned char type = 0;
	
	short result = 0;

	char *dataPtr = NULL;
	char numBuf[10];
	int num = 0;
	
	type = MQTT_UnPacketRecv(cmd);
	switch(type)
	{
		case MQTT_PKT_CMD:		//命令下发
			
			result = MQTT_UnPacketCmd(cmd, &cmdid_topic, &req_payload, &req_len);	//解topic和消息内容
			if(result == 0)
			{
				if(MQTT_PacketCmdResp(cmdid_topic, req_payload, &mqttPacket) == 0)	//命令回复打包
				{	
					ESP8266_SendData(mqttPacket._data, mqttPacket._len);			//回复命令
					MQTT_DeleteBuffer(&mqttPacket);			//删除
				}
			}
		
			break;
			
		case MQTT_PKT_PUBACK:			//发送Publish消息，平台回复的Ack
			
			break;
		
		default:
			result = -1;
			break;
	}
	
	ESP8266_Clear();									//清空缓存
	
	if(result == -1)
		return;
	
	dataPtr = strchr(req_payload, '}');					//搜索'}'

	if(dataPtr != NULL && result != -1)					//如果找到了
	{
		dataPtr++;
		
		while(*dataPtr >= '0' && *dataPtr <= '9')		//判断是否是命令下发后的参数
		{
			numBuf[num++] = *dataPtr++;
		}
		
		num = atoi((const char *)numBuf);				//转为数值格式
	}

	if(type == MQTT_PKT_CMD || type == MQTT_PKT_PUBLISH)
	{
		MQTT_FreeBuffer(cmdid_topic);
		MQTT_FreeBuffer(req_payload);
	}
}

//==========================================================
//	函数名称:	OneNet_SetControl
//
//	函数功能:	设置控制设备状态
//
//	入口参数:	fan:风扇状态, valve:电磁阀状态, alarm:报警状态
//
//	返回参数:	无
//
//	说明:		
//==========================================================
void OneNet_SetControl(uint8_t fan, uint8_t valve, uint8_t alarm)
{
    control_fan = fan;
    control_valve = valve;
    control_alarm = alarm;
}

//==========================================================
//	函数名称:	OneNet_GetControl
//
//	函数功能:	获取控制设备状态
//
//	入口参数:	fan:风扇状态指针, valve:电磁阀状态指针, alarm:报警状态指针
//
//	返回参数:	无
//
//	说明:		
//==========================================================
void OneNet_GetControl(uint8_t *fan, uint8_t *valve, uint8_t *alarm)
{
    if(fan) *fan = control_fan;
    if(valve) *valve = control_valve;
    if(alarm) *alarm = control_alarm;
}
