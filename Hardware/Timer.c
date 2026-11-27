#include "Timer.h"

volatile uint8_t  speed_update_flag = 0u;
volatile uint32_t system_ms = 0u;

static uint16_t speed_ms_counter = 0u;

void Timer_Init(void)
{
    if (SysTick_Config(SystemCoreClock / 1000u) != 0u)
    {
        while (1)
        {
        }
    }
}

void SysTick_Handler(void)
{
    system_ms++;

    speed_ms_counter++;
    if (speed_ms_counter >= CONTROL_PERIOD_MS)
    {
        speed_ms_counter = 0u;
        speed_update_flag = 1u;
    }
}

uint32_t Timer_GetMs(void)
{
    return system_ms;
}
