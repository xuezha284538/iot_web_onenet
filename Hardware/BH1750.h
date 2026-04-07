#ifndef __BH1750_H
#define __BH1750_H

#include "stm32f10x.h"

void BH1750_Init(void);
void BH1750_PowerOn(void);
void BH1750_PowerDown(void);
void BH1750_Reset(void);
void BH1750_SetMode(uint8_t mode);
float BH1750_ReadLightIntensity(void);

#endif
