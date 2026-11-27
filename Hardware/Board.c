#include "Board.h"

/*
 * C30D V2.2:
 * PE8 = user LED, active low.
 * PD3 = enable switch, active low (switch to GND).
 */
void Board_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE | RCC_AHB1Periph_GPIOD,
                           ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOE, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOD, &GPIO_InitStructure);

    Board_SetLed(0u);
}

void Board_SetLed(uint8_t on)
{
    if (on)
    {
        GPIO_ResetBits(GPIOE, GPIO_Pin_8);
    }
    else
    {
        GPIO_SetBits(GPIOE, GPIO_Pin_8);
    }
}

uint8_t Board_IsEnabled(void)
{
    return (uint8_t)(GPIO_ReadInputDataBit(GPIOD, GPIO_Pin_3) == Bit_RESET);
}
