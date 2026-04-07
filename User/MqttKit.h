#ifndef _MQTTKIT_H_
#define _MQTTKIT_H_


#include "Common.h"


//=============================内存管理==============================
//===========如果系统提供RTOS的内存管理接口，也可以使用C库=========
//RTOS
#include <stdlib.h>

#define MQTT_MallocBuffer	malloc

#define MQTT_FreeBuffer		free
//==========================================================


#define MOSQ_MSB(A)         (uint8)((A & 0xFF00) >> 8)
#define MOSQ_LSB(A)         (uint8)(A & 0x00FF)


/*--------------------------------内存分配方式标志--------------------------------*/
#define MEM_FLAG_NULL		0
#define MEM_FLAG_ALLOC		1
#define MEM_FLAG_STATIC		2


typedef struct Buffer
{
	
	uint8	*_data;		// 协议数据
	
	uint32	_len;		// 写入的数据长度
	
	uint32	_size;		// 缓存总大小
	
	uint8	_memFlag;	// 内存使用的方式：0-未分配	1-使用的动态内存		2-使用的固定内存
	
} MQTT_PACKET_STRUCTURE;


/*--------------------------------固定头类型信息定义--------------------------------*/
enum MqttPacketType
{
	
    MQTT_PKT_CONNECT = 1, /**< 连接请求类型数据包 */
    MQTT_PKT_CONNACK,     /**< 连接确认类型数据包 */
    MQTT_PKT_PUBLISH,     /**< 发布消息类型数据包 */
    MQTT_PKT_PUBACK,      /**< 发布确认类型数据包 */
    MQTT_PKT_PUBREC,      /**< 发布消息已接收数据包（Qos 2时，回复MQTT_PKT_PUBLISH） */
    MQTT_PKT_PUBREL,      /**< 发布消息已释放数据包（Qos 2时，回复MQTT_PKT_PUBREC） */
    MQTT_PKT_PUBCOMP,     /**< 发布消息完成数据包（Qos 2时，回复MQTT_PKT_PUBREL） */
    MQTT_PKT_SUBSCRIBE,   /**< 订阅类型数据包 */
    MQTT_PKT_SUBACK,      /**< 订阅确认类型数据包 */
    MQTT_PKT_UNSUBSCRIBE, /**< 取消订阅类型数据包 */
    MQTT_PKT_UNSUBACK,    /**< 取消订阅确认类型数据包 */
    MQTT_PKT_PINGREQ,     /**< ping 请求数据包 */
    MQTT_PKT_PINGRESP,    /**< ping 响应数据包 */
    MQTT_PKT_DISCONNECT,  /**< 断开连接数据包 */
	
	//其他
	
	MQTT_PKT_CMD  		 /**< 命令路由类型数据包 */
	
};


/*--------------------------------MQTT QOS等级--------------------------------*/
enum MqttQosLevel
{
	
    MQTT_QOS_LEVEL0,  /**< 最多发送一次 */
    MQTT_QOS_LEVEL1,  /**< 至少发送一次  */
    MQTT_QOS_LEVEL2   /**< 只接收一次 */
	
};


/*--------------------------------MQTT 连接参数标志位（内部使用）--------------------------------*/
enum MqttConnectFlag
{
	
    MQTT_CONNECT_CLEAN_SESSION  = 0x02,
    MQTT_CONNECT_WILL_FLAG      = 0x04,
    MQTT_CONNECT_WILL_QOS0      = 0x00,
    MQTT_CONNECT_WILL_QOS1      = 0x08,
    MQTT_CONNECT_WILL_QOS2      = 0x10,
    MQTT_CONNECT_WILL_RETAIN    = 0x20,
    MQTT_CONNECT_PASSORD        = 0x40,
    MQTT_CONNECT_USER_NAME      = 0x80
	
};


/*--------------------------------消息体packet ID，可自定义--------------------------------*/
//#define MQTT_PUBLISH_ID			10
#define MQTT_PUBLISH_ID			10

#define MQTT_SUBSCRIBE_ID		20

#define MQTT_UNSUBSCRIBE_ID		30


/*--------------------------------删除缓存--------------------------------*/
void MQTT_DeleteBuffer(MQTT_PACKET_STRUCTURE *mqttPacket);

/*--------------------------------解包--------------------------------*/
uint8 MQTT_UnPacketRecv(uint8 *dataPtr);

/*--------------------------------登录组包--------------------------------*/
uint8 MQTT_PacketConnect(const int8 *user, const int8 *password, const int8 *devid,
						uint16 cTime, uint1 clean_session, uint1 qos,
						const int8 *will_topic, const int8 *will_msg, int32 will_retain,
						MQTT_PACKET_STRUCTURE *mqttPacket);

/*--------------------------------断开连接组包--------------------------------*/
//uint1 MQTT_PacketDisConnect(MQTT_PACKET_STRUCTURE *mqttPacket);

/*--------------------------------连接响应解包--------------------------------*/
uint8 MQTT_UnPacketConnectAck(uint8 *rev_data);

/*--------------------------------数据点上传组包--------------------------------*/
uint1 MQTT_PacketSaveData(const int8 *devid, int16 send_len, int8 *type_bin_head, uint8 type, MQTT_PACKET_STRUCTURE *mqttPacket);

/*--------------------------------命令路由解包--------------------------------*/
uint8 MQTT_UnPacketCmd(uint8 *rev_data, int8 **cmdid, int8 **req, uint16 *req_len);

/*--------------------------------命令回复组包--------------------------------*/
uint1 MQTT_PacketCmdResp(const int8 *cmdid, const int8 *req, MQTT_PACKET_STRUCTURE *mqttPacket);

/*--------------------------------订阅消息组包--------------------------------*/
//uint8 MQTT_PacketSubscribe(uint16 pkt_id, enum MqttQosLevel qos, const int8 *topics[], uint8 topics_cnt, MQTT_PACKET_STRUCTURE *mqttPacket);

/*--------------------------------订阅消息回复解包--------------------------------*/
//uint8 MQTT_UnPacketSubscribe(uint8 *rev_data);

/*--------------------------------取消订阅组包--------------------------------*/
//uint8 MQTT_PacketUnSubscribe(uint16 pkt_id, const int8 *topics[], uint8 topics_cnt, MQTT_PACKET_STRUCTURE *mqttPacket);

/*--------------------------------取消订阅的回复解包--------------------------------*/
//uint1 MQTT_UnPacketUnSubscribe(uint8 *rev_data);

/*--------------------------------发布消息组包--------------------------------*/
uint8 MQTT_PacketPublish(uint16 pkt_id, const int8 *topic,
						const int8 *payload, uint32 payload_len,
						enum MqttQosLevel qos, int32 retain, int32 own,
						MQTT_PACKET_STRUCTURE *mqttPacket);

/*--------------------------------发布消息回复解包--------------------------------*/
uint8 MQTT_UnPacketPublish(uint8 *rev_data, int8 **topic, uint16 *topic_len, int8 **payload, uint16 *payload_len, uint8 *qos, uint16 *pkt_id);

/*--------------------------------发布消息的Ack组包--------------------------------*/
//uint1 MQTT_PacketPublishAck(uint16 pkt_id, MQTT_PACKET_STRUCTURE *mqttPacket);

/*--------------------------------发布消息的Ack解包--------------------------------*/
uint1 MQTT_UnPacketPublishAck(uint8 *rev_data);

/*--------------------------------发布消息的Rec组包--------------------------------*/
//uint1 MQTT_PacketPublishRec(uint16 pkt_id, MQTT_PACKET_STRUCTURE *mqttPacket);

/*--------------------------------发布消息的Rec解包--------------------------------*/
//uint1 MQTT_UnPacketPublishRec(uint8 *rev_data);

/*--------------------------------发布消息的Rel组包--------------------------------*/
//uint1 MQTT_PacketPublishRel(uint16 pkt_id, MQTT_PACKET_STRUCTURE *mqttPacket);

/*--------------------------------发布消息的Rel解包--------------------------------*/
//uint1 MQTT_UnPacketPublishRel(uint8 *rev_data, uint16 pkt_id);

/*--------------------------------发布消息的Comp组包--------------------------------*/
//uint1 MQTT_PacketPublishComp(uint16 pkt_id, MQTT_PACKET_STRUCTURE *mqttPacket);

/*--------------------------------发布消息的Comp解包--------------------------------*/
//uint1 MQTT_UnPacketPublishComp(uint8 *rev_data);

/*--------------------------------心跳请求组包--------------------------------*/
//uint1 MQTT_PacketPing(MQTT_PACKET_STRUCTURE *mqttPacket);


#endif
