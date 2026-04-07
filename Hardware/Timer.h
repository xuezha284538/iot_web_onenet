/**
	**********************************************************************
	* @file    Timer.h
	* @brief   定时器头文件
	**********************************************************************
**/

#ifndef __TIMER_H
#define __TIMER_H

#include "stm32f10x.h"

// 全局变量声明
extern volatile uint8_t timer_1s_flag;
extern volatile uint32_t reconnect_timer;  // 改为uint32支持30秒计时

// 函数声明
void TIM3_Init(void);
uint8_t GetReconnectFlag(void);

#endif
