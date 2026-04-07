#include "stm32f10x.h"                  // Device header
#include "Delay.h"

#include "DHT11.h"

#define DHT11_GPIO GPIOB
#define DHT11_PIN GPIO_Pin_14

// 初始化为输出
void DHT11_GPIO_OUT(void)
{
	GPIO_InitTypeDef  GPIO_InitStructure;
	
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB,ENABLE);    // 使能时钟GPIO时钟在APB2
		
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_14;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; // 推挽输出，输出高低电平有效
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);		

}

// 初始化为输入
void DHT11_GPIO_IN(void)
{
	GPIO_InitTypeDef  GPIO_InitStructure;
	
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB,ENABLE);
		
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_14;	
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING; // 浮空输入，读取高低电平
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);					
}

uint8_t DHT11_ReadByte(void)
{
	uint8_t temp;
	uint8_t ReadDat=0;
	
	uint8_t retry = 0;	
	uint8_t i;
	// 循环读取八位数据
	
	for(i=0;i<8;i++)
	{
		// 等待准备信号低电平50us
		while(GPIO_ReadInputDataBit(GPIOB,GPIO_Pin_14)==0&&retry<100)
		{
			Delay_us(1);	
			retry++;		
		}
		retry=0;
		
		// 数据0信号高电平时长为28us，retry循环是超时处理
		// 防止死循环，相当于如果一直读0的话可以避免死循环
		Delay_us(30);// 如果是0，则temp=0，不用管，如果是0，仍然在高电平时间内
		
		// 因为如果是0的话，高电平时长短，所以预设0简化代码
		temp=0;// 预设信号0，temp=0
		// 数据0信号高电平约28us，数据1信号高电平70us，延时30us后确定是0还是1
		if(GPIO_ReadInputDataBit(GPIOB,GPIO_Pin_14)==1) 
		{
			temp=1;
		}			
		// 数据1信号高电平剩余40us
		while(GPIO_ReadInputDataBit(GPIOB,GPIO_Pin_14)==1&&retry<100)
		{		
			Delay_us(1);
			retry++;
		}
		retry=0;
		// <<= 左移赋值运算符，|= 或然后赋值，得到第1位数据，循环8bit
		ReadDat<<=1;
		ReadDat|=temp;
	}	
	return ReadDat;
}

void DHT11_ReadData(uint8_t *temp, uint8_t *humi)
{
    uint8_t i,data[5];
	uint8_t retry=0;
    
	// stm32 PB14配置为输出，发送开始信号低电平18ms，高电平40us
	DHT11_GPIO_OUT();
	GPIO_ResetBits(GPIOB,GPIO_Pin_14);// 输出低电平
	Delay_ms(18);// 保证dht11能收到开始信号
	
	GPIO_SetBits(GPIOB,GPIO_Pin_14);// 输出高电平
	Delay_us(40);// 延时40us，等待dht11响应信号
	
	// stm32 PB14配置为输入，检测并读取响应信号低电平80us，高电平80us
	DHT11_GPIO_IN();// 切换输入模式，直接检测高电平，也就是开始检测
	Delay_us(20);
	// 延时20us，dht11响应低电平80us，还剩60us，检测是否是低电平，确定是否收到响应信号
		
	if(GPIO_ReadInputDataBit(GPIOB,GPIO_Pin_14)==0)
	{	
		// 第一个循环检测响应低电平60us
		while(GPIO_ReadInputDataBit(GPIOB,GPIO_Pin_14)==0&&retry<100)// 检测响应信号低电平剩余60us
		{
			Delay_us(1);
			retry++;		
		}
		
		retry=0;// 超过100us自动跳出循环，避免卡死
		// 检测dht11响应80us
		while(GPIO_ReadInputDataBit(GPIOB,GPIO_Pin_14)==1&&retry<100)// 检测响应信号高电平80us
		{
			Delay_us(1);
			retry++;			
		}
		retry=0;
		
		// 读取8字节数据，每次读1bit之前先检测低电平50us
		// 读取5位数据，40bit
		for(i=0;i<5;i++)
		{
			data[i]=DHT11_ReadByte();
		}
		Delay_us(50);// DHT11发送完成50us作为一字节结束信号，可以不用管
        if ((data[0] + data[1] + data[2] + data[3]) == data[4])
        {
            *humi = data[0];
            *temp = data[2];
        }	
	}
}
