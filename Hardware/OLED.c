#include "stm32f4xx.h"
#include "OLED.h"
#include "Timer.h"

/*
 * WHEELTEC C30D V2.2 OLED interface:
 * PD14 = SCLK, PD13 = DIN, PD11 = DC, PD12 = RST.
 * This is the mapping used by the board's hardware-validated example.
 */
#define OLED_DC_PIN    GPIO_Pin_11
#define OLED_RST_PIN   GPIO_Pin_12
#define OLED_DIN_PIN   GPIO_Pin_13
#define OLED_CLK_PIN   GPIO_Pin_14
#define OLED_ALL_PINS  (OLED_DC_PIN | OLED_RST_PIN | OLED_DIN_PIN | OLED_CLK_PIN)

static uint8_t oled_framebuffer[8][128];

static const uint8_t oled_glyphs[36][5] =
{
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, /* 0 */
    {0x00, 0x42, 0x7F, 0x40, 0x00}, /* 1 */
    {0x42, 0x61, 0x51, 0x49, 0x46}, /* 2 */
    {0x21, 0x41, 0x45, 0x4B, 0x31}, /* 3 */
    {0x18, 0x14, 0x12, 0x7F, 0x10}, /* 4 */
    {0x27, 0x45, 0x45, 0x45, 0x39}, /* 5 */
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, /* 6 */
    {0x01, 0x71, 0x09, 0x05, 0x03}, /* 7 */
    {0x36, 0x49, 0x49, 0x49, 0x36}, /* 8 */
    {0x06, 0x49, 0x49, 0x29, 0x1E}, /* 9 */
    {0x7E, 0x11, 0x11, 0x11, 0x7E}, /* A */
    {0x7F, 0x49, 0x49, 0x49, 0x36}, /* B */
    {0x3E, 0x41, 0x41, 0x41, 0x22}, /* C */
    {0x7F, 0x41, 0x41, 0x22, 0x1C}, /* D */
    {0x7F, 0x49, 0x49, 0x49, 0x41}, /* E */
    {0x7F, 0x09, 0x09, 0x09, 0x01}, /* F */
    {0x3E, 0x41, 0x49, 0x49, 0x7A}, /* G */
    {0x7F, 0x08, 0x08, 0x08, 0x7F}, /* H */
    {0x00, 0x41, 0x7F, 0x41, 0x00}, /* I */
    {0x20, 0x40, 0x41, 0x3F, 0x01}, /* J */
    {0x7F, 0x08, 0x14, 0x22, 0x41}, /* K */
    {0x7F, 0x40, 0x40, 0x40, 0x40}, /* L */
    {0x7F, 0x02, 0x0C, 0x02, 0x7F}, /* M */
    {0x7F, 0x04, 0x08, 0x10, 0x7F}, /* N */
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, /* O */
    {0x7F, 0x09, 0x09, 0x09, 0x06}, /* P */
    {0x3E, 0x41, 0x51, 0x21, 0x5E}, /* Q */
    {0x7F, 0x09, 0x19, 0x29, 0x46}, /* R */
    {0x46, 0x49, 0x49, 0x49, 0x31}, /* S */
    {0x01, 0x01, 0x7F, 0x01, 0x01}, /* T */
    {0x3F, 0x40, 0x40, 0x40, 0x3F}, /* U */
    {0x1F, 0x20, 0x40, 0x20, 0x1F}, /* V */
    {0x3F, 0x40, 0x38, 0x40, 0x3F}, /* W */
    {0x63, 0x14, 0x08, 0x14, 0x63}, /* X */
    {0x07, 0x08, 0x70, 0x08, 0x07}, /* Y */
    {0x61, 0x51, 0x49, 0x45, 0x43}  /* Z */
};

static void OLED_DelayMs(uint32_t milliseconds)
{
    uint32_t start = Timer_GetMs();
    while ((Timer_GetMs() - start) < milliseconds)
    {
    }
}

static void OLED_DelayEdge(void)
{
    volatile uint32_t count;
    for (count = 0u; count < 20u; count++)
    {
        __NOP();
    }
}

static void OLED_InitGpio(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);
    GPIO_ResetBits(GPIOD, OLED_ALL_PINS);

    GPIO_InitStructure.GPIO_Pin = OLED_ALL_PINS;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_Init(GPIOD, &GPIO_InitStructure);

    OLED_DelayMs(100u);
    GPIO_SetBits(GPIOD, OLED_RST_PIN);
    OLED_DelayMs(10u);
}

static void OLED_SendByte(uint8_t data, uint8_t is_data)
{
    uint8_t bit;

    if (is_data)
    {
        GPIO_SetBits(GPIOD, OLED_DC_PIN);
    }
    else
    {
        GPIO_ResetBits(GPIOD, OLED_DC_PIN);
    }

    for (bit = 0u; bit < 8u; bit++)
    {
        GPIO_ResetBits(GPIOD, OLED_CLK_PIN);
        if ((data & 0x80u) != 0u)
        {
            GPIO_SetBits(GPIOD, OLED_DIN_PIN);
        }
        else
        {
            GPIO_ResetBits(GPIOD, OLED_DIN_PIN);
        }
        OLED_DelayEdge();
        GPIO_SetBits(GPIOD, OLED_CLK_PIN);
        OLED_DelayEdge();
        data <<= 1;
    }
    GPIO_ResetBits(GPIOD, OLED_CLK_PIN);
}

static void OLED_WriteCommand(uint8_t command)
{
    OLED_SendByte(command, 0u);
}

static void OLED_WriteData(uint8_t data)
{
    OLED_SendByte(data, 1u);
}

static uint8_t OLED_ReverseBits(uint8_t value)
{
    uint8_t result = 0u;
    uint8_t bit;

    for (bit = 0u; bit < 8u; bit++)
    {
        result |= (uint8_t)(((value >> bit) & 0x01u) << (7u - bit));
    }
    return result;
}

void OLED_Clear(void)
{
    uint8_t row;
    uint8_t column;

    for (row = 0u; row < 8u; row++)
    {
        for (column = 0u; column < 128u; column++)
        {
            oled_framebuffer[row][column] = 0u;
        }
    }
}

void OLED_ShowChar(uint8_t line, uint8_t column, char character)
{
    uint8_t symbol[5] = {0u, 0u, 0u, 0u, 0u};
    const uint8_t *glyph = symbol;
    uint8_t index;
    uint8_t pixel;

    if ((line < 1u) || (line > 8u) || (column < 1u) || (column > 21u))
    {
        return;
    }

    if ((character >= 'a') && (character <= 'z'))
    {
        character = (char)(character - ('a' - 'A'));
    }

    if ((character >= '0') && (character <= '9'))
    {
        glyph = oled_glyphs[character - '0'];
    }
    else if ((character >= 'A') && (character <= 'Z'))
    {
        glyph = oled_glyphs[10u + (character - 'A')];
    }
    else if (character == '-')
    {
        symbol[0] = 0x08u;
        symbol[1] = 0x08u;
        symbol[2] = 0x08u;
        symbol[3] = 0x08u;
        symbol[4] = 0x08u;
    }
    else if (character == '+')
    {
        symbol[0] = 0x08u;
        symbol[1] = 0x08u;
        symbol[2] = 0x3Eu;
        symbol[3] = 0x08u;
        symbol[4] = 0x08u;
    }
    else if (character == ':')
    {
        symbol[0] = 0x00u;
        symbol[1] = 0x36u;
        symbol[2] = 0x36u;
        symbol[3] = 0x00u;
        symbol[4] = 0x00u;
    }
    else if (character == '.')
    {
        symbol[2] = 0x60u;
    }
    else if (character == '/')
    {
        symbol[0] = 0x20u;
        symbol[1] = 0x10u;
        symbol[2] = 0x08u;
        symbol[3] = 0x04u;
        symbol[4] = 0x02u;
    }

    index = (uint8_t)((column - 1u) * 6u);
    for (pixel = 0u; pixel < 5u; pixel++)
    {
        oled_framebuffer[line - 1u][index + pixel] =
            OLED_ReverseBits(glyph[pixel]);
    }
}

void OLED_ShowString(uint8_t line, uint8_t column, char *string)
{
    uint8_t index = 0u;

    while ((string[index] != '\0') &&
           ((uint8_t)(column + index) <= 21u))
    {
        OLED_ShowChar(line, (uint8_t)(column + index), string[index]);
        index++;
    }
}

uint32_t OLED_Pow(uint32_t base, uint32_t exponent)
{
    uint32_t result = 1u;

    while (exponent > 0u)
    {
        result *= base;
        exponent--;
    }
    return result;
}

void OLED_ShowNum(uint8_t line, uint8_t column, uint32_t number,
                  uint8_t length)
{
    uint8_t index;

    for (index = 0u; index < length; index++)
    {
        OLED_ShowChar(line, (uint8_t)(column + index),
                      (char)('0' + ((number / OLED_Pow(10u,
                          (uint32_t)(length - index - 1u))) % 10u)));
    }
}

void OLED_ShowSignedNum(uint8_t line, uint8_t column, int32_t number,
                        uint8_t length)
{
    uint32_t magnitude;

    if (number < 0)
    {
        OLED_ShowChar(line, column, '-');
        magnitude = (uint32_t)(-number);
    }
    else
    {
        OLED_ShowChar(line, column, '+');
        magnitude = (uint32_t)number;
    }
    OLED_ShowNum(line, (uint8_t)(column + 1u), magnitude, length);
}

void OLED_Refresh(void)
{
    uint8_t row;
    uint16_t column;

    for (row = 0u; row < 8u; row++)
    {
        OLED_WriteCommand((uint8_t)(0xB0u + 7u - row));
        OLED_WriteCommand(0x00u);
        OLED_WriteCommand(0x10u);
        for (column = 0u; column < 128u; column++)
        {
            OLED_WriteData(oled_framebuffer[row][column]);
        }
    }
}

void OLED_Init(void)
{
    static const uint8_t init_commands[] =
    {
        0xAEu, 0xD5u, 0x80u, 0xA8u, 0x3Fu, 0xD3u, 0x00u, 0x40u,
        0x8Du, 0x14u, 0x20u, 0x02u, 0xA1u, 0xC0u, 0xDAu, 0x12u,
        0x81u, 0x7Fu, 0xD9u, 0xF1u, 0xDBu, 0x30u, 0xA4u, 0xA6u
    };
    uint8_t index;

    OLED_InitGpio();
    for (index = 0u; index < sizeof(init_commands); index++)
    {
        OLED_WriteCommand(init_commands[index]);
    }

    OLED_Clear();
    OLED_Refresh();
    OLED_WriteCommand(0xAFu);
}
