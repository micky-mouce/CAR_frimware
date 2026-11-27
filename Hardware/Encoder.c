#include "Encoder.h"
#include "Motor.h"
#include "BoardConfig.h"

static TIM_TypeDef * const encoder_timer[MOTOR_COUNT] =
{
    TIM2, TIM3, TIM4, TIM5
};

static int16_t encoder_last_count[MOTOR_COUNT] = {0, 0, 0, 0};

static void Encoder_InitTimer(TIM_TypeDef *timer, uint32_t period)
{
    TIM_TimeBaseInitTypeDef time_base;
    TIM_ICInitTypeDef input_capture;

    TIM_TimeBaseStructInit(&time_base);
    time_base.TIM_Prescaler = 0u;
    time_base.TIM_CounterMode = TIM_CounterMode_Up;
    time_base.TIM_Period = period;
    time_base.TIM_ClockDivision = TIM_CKD_DIV1;
    time_base.TIM_RepetitionCounter = 0u;
    TIM_TimeBaseInit(timer, &time_base);

    TIM_EncoderInterfaceConfig(timer, TIM_EncoderMode_TI12,
                               TIM_ICPolarity_Rising,
                               TIM_ICPolarity_Rising);

    TIM_ICStructInit(&input_capture);
    input_capture.TIM_Channel = TIM_Channel_1;
    input_capture.TIM_ICFilter = 10u;
    TIM_ICInit(timer, &input_capture);
    input_capture.TIM_Channel = TIM_Channel_2;
    TIM_ICInit(timer, &input_capture);

    TIM_SetCounter(timer, 0u);
    TIM_Cmd(timer, ENABLE);
}

void Encoder_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    uint8_t index;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA | RCC_AHB1Periph_GPIOB,
                           ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2 | RCC_APB1Periph_TIM3 |
                           RCC_APB1Periph_TIM4 | RCC_APB1Periph_TIM5,
                           ENABLE);

    GPIO_PinAFConfig(GPIOA, GPIO_PinSource15, GPIO_AF_TIM2);
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource3, GPIO_AF_TIM2);
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource4, GPIO_AF_TIM3);
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource5, GPIO_AF_TIM3);
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource6, GPIO_AF_TIM4);
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource7, GPIO_AF_TIM4);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource0, GPIO_AF_TIM5);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource1, GPIO_AF_TIM5);

    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_15;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3 | GPIO_Pin_4 | GPIO_Pin_5 |
                                  GPIO_Pin_6 | GPIO_Pin_7;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    Encoder_InitTimer(TIM2, 0xFFFFFFFFu);
    Encoder_InitTimer(TIM3, 0xFFFFu);
    Encoder_InitTimer(TIM4, 0xFFFFu);
    Encoder_InitTimer(TIM5, 0xFFFFu);

    for (index = 0u; index < MOTOR_COUNT; index++)
    {
        encoder_last_count[index] = 0;
    }
}

int16_t Encoder_GetDelta(uint8_t encoder_index)
{
    int16_t now;
    int16_t delta;

    if (encoder_index >= MOTOR_COUNT)
    {
        return 0;
    }

    now = (int16_t)TIM_GetCounter(encoder_timer[encoder_index]);
    delta = (int16_t)(now - encoder_last_count[encoder_index]);
    encoder_last_count[encoder_index] = now;
    return delta;
}

float Encoder_GetRPM(uint8_t encoder_index, uint16_t period_ms)
{
    static const int8_t encoder_sign[MOTOR_COUNT] =
    {
        ENCODER_SIGN_A, ENCODER_SIGN_B, ENCODER_SIGN_C, ENCODER_SIGN_D
    };
    int16_t delta;
    float counts_per_output_rev;

    if (encoder_index >= MOTOR_COUNT)
    {
        return 0.0f;
    }

    delta = Encoder_GetDelta(encoder_index);
    delta *= encoder_sign[encoder_index];
    counts_per_output_rev = ENCODER_PPR * 4.0f * GEAR_RATIO;

    return ((float)delta / counts_per_output_rev) *
           (60000.0f / (float)period_ms);
}
