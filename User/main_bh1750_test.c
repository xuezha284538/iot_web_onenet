#include "stm32f10x.h"
#include "BH1750.h"
#include "OLED.h"
#include "Delay.h"

int main(void)
{
	float light_intensity;
	
	OLED_Init();
	BH1750_Init();
	
	OLED_ShowString(1, 1, "BH1750 Test");
	OLED_ShowString(2, 1, "Light:");
	
	while(1)
	{
		light_intensity = BH1750_ReadLightIntensity();
		
		OLED_ShowNum(3, 1, (uint32_t)light_intensity, 5);
		OLED_ShowString(3, 6, "lx");
		
		Delay_ms(500);
	}
}
