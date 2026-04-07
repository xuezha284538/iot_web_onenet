#ifndef __UART_H
#define __UART_H

#include "stm32f10x.h"                  // Device header

void USART_Configuration(void);
void USART_SendChar(uint8_t ch);
void USART_SendString(char *str);

#endif
