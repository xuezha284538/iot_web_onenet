#include "stm32f10x.h"
#include "Delay.h"
#include "OLED.h"
#include "DHT11.h"
#include "BH1750.h"
#include "MQ2.h"

uint8_t temp = 0, humi = 0;
float light_lux;
uint16_t smoke_val;
uint8_t last_temp = 0, last_humi = 0;
uint16_t last_light = 0;
uint16_t last_smoke = 0;

int main(void)
{
    OLED_Init();
    BH1750_Init();
    MQ2_Init();
    OLED_Clear();
    uint16_t dht_timer = 0;
    uint16_t light_timer = 0;
    uint16_t smoke_timer = 0;
    
    OLED_ShowString(1,1,"temp:");
    OLED_ShowString(2,1,"Hum :");
    OLED_ShowString(3,1,"Light:");
    OLED_ShowString(4,1,"Smoke:");
    OLED_ShowString(2,8,"%RH");
    OLED_ShowString(3,10,"lx");
    
    while(1)
    {
        // 每 50ms 读一次光照强度
        if(++light_timer >= 50)
        {
            light_timer = 0;
            light_lux = BH1750_ReadLightIntensity();
        }
        
        // 每 100ms 读一次烟雾
        if(++smoke_timer >= 100)
        {
            smoke_timer = 0;
            smoke_val = MQ2_Read();
        }

        // OLED 只在变化时刷新
        if(temp != last_temp || humi != last_humi || (uint16_t)light_lux != last_light || smoke_val != last_smoke)
        {
            last_temp = temp;
            last_humi = humi;
            last_light = (uint16_t)light_lux;
            last_smoke = smoke_val;
            
            OLED_ShowNum(1,6,temp,2);
            OLED_ShowString(1,8,"C");
            OLED_ShowNum(2,6,humi,2);
            OLED_ShowNum(3,7,(uint16_t)light_lux,4);
            OLED_ShowNum(4,7,smoke_val,4);
            
            // 显示烟雾等级 L/M/H
            if(smoke_val <= 1500)
            {
                OLED_ShowString(4,12,"L ");
            }
            else if(smoke_val <= 3000)
            {
                OLED_ShowString(4,12,"M ");
            }
            else
            {
                OLED_ShowString(4,12,"H ");
            }
        }
        
        // DHT11 100ms 读一次
        Delay_ms(10);
        if(++dht_timer >= 10)
        {
            dht_timer = 0;
            DHT11_ReadData(&temp, &humi);
        }
    }
}
