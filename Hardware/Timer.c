/**
	**********************************************************************
	* @file    Timer.c
	* @brief   定时器初始化（用于OneNET重连）
	**********************************************************************
**/

#include "stm32f10x.h"
#include "Timer.h"

// 全局变量
volatile uint8_t timer_1s_flag = 0;     // 1秒定时标志
volatile uint32_t reconnect_timer = 0;   // 重连计时器(改为uint32防止溢出)

// ==================== TIM3 初始化（1ms中断）====================
void TIM3_Init(void)
{
	TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
	NVIC_InitTypeDef NVIC_InitStructure;
	
	// 开启TIM3时钟 - TIM3属于APB1总线
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);
	
	// TIM3 配置：1MHz时钟，1ms中断
	// 系统时钟72MHz，APB1=36MHz，TIM3×2=72MHz
	// 72000000 / (71+1) / 1000 = 1000Hz → 1ms
	TIM_TimeBaseStructure.TIM_Period = 999;          // 计数值：1000-1
	TIM_TimeBaseStructure.TIM_Prescaler = 71;       // 预分频：72M/72=1MHz
	TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure);
	
	// 使能更新中断
	TIM_ITConfig(TIM3, TIM_IT_Update, ENABLE);
	
	// NVIC配置
	NVIC_InitStructure.NVIC_IRQChannel = TIM3_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
	
	// 启动定时器
	TIM_Cmd(TIM3, ENABLE);
}

// ==================== 获取重连标志 =====================
uint8_t GetReconnectFlag(void)
{
	if(reconnect_timer >= 30000)  // 30秒才触发一次重连检测
	{
		reconnect_timer = 0;
		return 1;
	}
	return 0;
}
