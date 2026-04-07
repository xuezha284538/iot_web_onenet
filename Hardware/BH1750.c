#include "stm32f10x.h"
#include "Delay.h"
#include "BH1750.h"

#define BH1750_ADDR 0x46

#define BH1750_POWER_DOWN 0x00
#define BH1750_POWER_ON 0x01
#define BH1750_RESET 0x07
#define BH1750_CON_H_RES_MODE 0x10
#define BH1750_CON_H_RES_MODE2 0x11
#define BH1750_CON_L_RES_MODE 0x13
#define BH1750_ONE_TIME_H_RES_MODE 0x20
#define BH1750_ONE_TIME_H_RES_MODE2 0x21
#define BH1750_ONE_TIME_L_RES_MODE 0x23

void BH1750_I2C_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	I2C_InitTypeDef I2C_InitStructure;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1, ENABLE);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_OD;
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	I2C_InitStructure.I2C_Mode = I2C_Mode_I2C;
	I2C_InitStructure.I2C_DutyCycle = I2C_DutyCycle_2;
	I2C_InitStructure.I2C_OwnAddress1 = 0x00;
	I2C_InitStructure.I2C_Ack = I2C_Ack_Enable;
	I2C_InitStructure.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
	I2C_InitStructure.I2C_ClockSpeed = 100000;
	I2C_Init(I2C1, &I2C_InitStructure);

	I2C_Cmd(I2C1, ENABLE);
}

void BH1750_WriteByte(uint8_t data)
{
	while(I2C_GetFlagStatus(I2C1, I2C_FLAG_BUSY));

	I2C_GenerateSTART(I2C1, ENABLE);
	while(!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_MODE_SELECT));

	I2C_Send7bitAddress(I2C1, BH1750_ADDR, I2C_Direction_Transmitter);
	while(!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED));

	I2C_SendData(I2C1, data);
	while(!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_TRANSMITTED));

	I2C_GenerateSTOP(I2C1, ENABLE);
}

void BH1750_ReadBytes(uint8_t *buf, uint8_t len)
{
	while(I2C_GetFlagStatus(I2C1, I2C_FLAG_BUSY));

	I2C_GenerateSTART(I2C1, ENABLE);
	while(!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_MODE_SELECT));

	I2C_Send7bitAddress(I2C1, BH1750_ADDR + 1, I2C_Direction_Receiver);
	while(!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED));

	while(len)
	{
		if(len == 1)
		{
			I2C_AcknowledgeConfig(I2C1, DISABLE);
			I2C_GenerateSTOP(I2C1, ENABLE);
		}
		while(!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_RECEIVED));
		*buf = I2C_ReceiveData(I2C1);
		buf++;
		len--;
	}
	I2C_AcknowledgeConfig(I2C1, ENABLE);
}

void BH1750_Init(void)
{
	BH1750_I2C_Init();
	BH1750_PowerOn();
	Delay_ms(10);
	BH1750_SetMode(BH1750_CON_H_RES_MODE);
	Delay_ms(180);
}

void BH1750_PowerOn(void)
{
	BH1750_WriteByte(BH1750_POWER_ON);
}

void BH1750_PowerDown(void)
{
	BH1750_WriteByte(BH1750_POWER_DOWN);
}

void BH1750_Reset(void)
{
	BH1750_PowerOn();
	Delay_ms(10);
	BH1750_WriteByte(BH1750_RESET);
}

void BH1750_SetMode(uint8_t mode)
{
	BH1750_WriteByte(mode);
}

float BH1750_ReadLightIntensity(void)
{
	uint8_t buf[2];
	uint16_t lux;
	
	BH1750_ReadBytes(buf, 2);
	
	lux = (buf[0] << 8) | buf[1];
	
	return (float)lux / 1.2;
}
