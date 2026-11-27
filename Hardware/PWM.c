#include "PWM.h"

#define PWM_PERIOD  8399u

static void PWM_InitTimer(TIM_TypeDef *timer)
{
    TIM_TimeBaseInitTypeDef time_base;
    TIM_OCInitTypeDef output_compare;

    TIM_TimeBaseStructInit(&time_base);
    time_base.TIM_Prescaler = 0u;
    time_base.TIM_CounterMode = TIM_CounterMode_Up;
    time_base.TIM_Period = PWM_PERIOD;
    time_base.TIM_ClockDivision = TIM_CKD_DIV1;
    time_base.TIM_RepetitionCounter = 0u;
    TIM_TimeBaseInit(timer, &time_base);

    TIM_OCStructInit(&output_compare);
    output_compare.TIM_OCMode = TIM_OCMode_PWM1;
    output_compare.TIM_OutputState = TIM_OutputState_Enable;
    output_compare.TIM_OCPolarity = TIM_OCPolarity_High;
    output_compare.TIM_Pulse = 0u;
    TIM_OC1Init(timer, &output_compare);
    TIM_OC1PreloadConfig(timer, TIM_OCPreload_Enable);

    if ((timer == TIM9) || (timer == TIM1))
    {
        TIM_OC2Init(timer, &output_compare);
        TIM_OC2PreloadConfig(timer, TIM_OCPreload_Enable);
    }

    if (timer == TIM1)
    {
        TIM_OC3Init(timer, &output_compare);
        TIM_OC3PreloadConfig(timer, TIM_OCPreload_Enable);
        TIM_OC4Init(timer, &output_compare);
        TIM_OC4PreloadConfig(timer, TIM_OCPreload_Enable);
        TIM_CtrlPWMOutputs(TIM1, ENABLE);
    }

    TIM_ARRPreloadConfig(timer, ENABLE);
    TIM_Cmd(timer, ENABLE);
}

void PWM_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA | RCC_AHB1Periph_GPIOB |
                           RCC_AHB1Periph_GPIOE, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1 | RCC_APB2Periph_TIM9 |
                           RCC_APB2Periph_TIM10 | RCC_APB2Periph_TIM11,
                           ENABLE);

    GPIO_PinAFConfig(GPIOB, GPIO_PinSource8, GPIO_AF_TIM10);
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource9, GPIO_AF_TIM11);
    GPIO_PinAFConfig(GPIOE, GPIO_PinSource5, GPIO_AF_TIM9);
    GPIO_PinAFConfig(GPIOE, GPIO_PinSource6, GPIO_AF_TIM9);
    GPIO_PinAFConfig(GPIOE, GPIO_PinSource9, GPIO_AF_TIM1);
    GPIO_PinAFConfig(GPIOE, GPIO_PinSource11, GPIO_AF_TIM1);
    GPIO_PinAFConfig(GPIOE, GPIO_PinSource13, GPIO_AF_TIM1);
    GPIO_PinAFConfig(GPIOE, GPIO_PinSource14, GPIO_AF_TIM1);

    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_DOWN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8 | GPIO_Pin_9;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_9 |
                                  GPIO_Pin_11 | GPIO_Pin_13 | GPIO_Pin_14;
    GPIO_Init(GPIOE, &GPIO_InitStructure);

    PWM_InitTimer(TIM10);
    PWM_InitTimer(TIM11);
    PWM_InitTimer(TIM9);
    PWM_InitTimer(TIM1);
}

void PWM_SetDuty(PWM_Channel channel, uint16_t duty)
{
    if (duty > PWM_PERIOD)
    {
        duty = PWM_PERIOD;
    }

    switch (channel)
    {
        case PWM_CH_A_POS: TIM_SetCompare1(TIM10, duty); break;
        case PWM_CH_A_NEG: TIM_SetCompare1(TIM11, duty); break;
        case PWM_CH_B_POS: TIM_SetCompare1(TIM9, duty);  break;
        case PWM_CH_B_NEG: TIM_SetCompare2(TIM9, duty);  break;
        case PWM_CH_C_POS: TIM_SetCompare1(TIM1, duty);  break;
        case PWM_CH_C_NEG: TIM_SetCompare2(TIM1, duty);  break;
        case PWM_CH_D_POS: TIM_SetCompare3(TIM1, duty);  break;
        case PWM_CH_D_NEG: TIM_SetCompare4(TIM1, duty);  break;
        default: break;
    }
}
