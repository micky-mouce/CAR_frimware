#include "ROSBridge.h"
#include "Timer.h"

#define RX_BUFFER_SIZE  128u
#define RX_LINE_SIZE    64u
#define ROS_PORT_COUNT  2u

typedef struct
{
    volatile uint8_t  buffer[RX_BUFFER_SIZE];
    volatile uint16_t head;
    volatile uint16_t tail;
    volatile uint32_t errors;
    char     line[RX_LINE_SIZE];
    uint16_t line_length;
    uint8_t  line_discard;
} ROS_RxContext;

static ROS_RxContext rx_context[ROS_PORT_COUNT];
static USART_TypeDef * const ros_uart[ROS_PORT_COUNT] = {USART1, USART3};

static uint32_t status_sequence = 0u;
static uint8_t  has_valid_command = 0u;
static uint32_t last_valid_command_ms = 0u;

static uint8_t ROSBridge_ReadByte(ROS_RxContext *context, uint8_t *data)
{
    if (context->tail == context->head)
    {
        return 0u;
    }

    *data = context->buffer[context->tail];
    context->tail = (uint16_t)((context->tail + 1u) % RX_BUFFER_SIZE);
    return 1u;
}

static void ROSBridge_WriteByte(USART_TypeDef *usart, char data)
{
    while (USART_GetFlagStatus(usart, USART_FLAG_TXE) == RESET)
    {
    }
    USART_SendData(usart, (uint16_t)(uint8_t)data);
}

static void ROSBridge_SendString(const char *text)
{
    uint8_t port;

    while (*text != '\0')
    {
        /* Feed both UARTs together so transmission time is not doubled. */
        for (port = 0u; port < ROS_PORT_COUNT; port++)
        {
            ROSBridge_WriteByte(ros_uart[port], *text);
        }
        text++;
    }
}

static char ROSBridge_ToUpper(char value)
{
    if ((value >= 'a') && (value <= 'z'))
    {
        return (char)(value - ('a' - 'A'));
    }
    return value;
}

static uint8_t ROSBridge_EqualsIgnoreCase(const char *left, const char *right)
{
    while ((*left != '\0') && (*right != '\0'))
    {
        if (ROSBridge_ToUpper(*left) != ROSBridge_ToUpper(*right))
        {
            return 0u;
        }
        left++;
        right++;
    }

    return (uint8_t)((*left == '\0') && (*right == '\0'));
}

static char *ROSBridge_SkipSpaces(char *text)
{
    while ((*text == ' ') || (*text == '\t'))
    {
        text++;
    }
    return text;
}

static uint8_t ROSBridge_ProcessLine(char *line, ROS_Command *command)
{
    char *end;
    uint8_t result = 0u;

    line = ROSBridge_SkipSpaces(line);
    end = line;
    while (*end != '\0')
    {
        end++;
    }
    while ((end > line) && ((end[-1] == ' ') || (end[-1] == '\t')))
    {
        end--;
    }
    *end = '\0';

    if (ROSBridge_EqualsIgnoreCase(line, "STOP"))
    {
        command->command = ROS_CMD_STOP;
        command->motion = MOTION_STOP;
        command->target_rpm = ROS_DEFAULT_TARGET_RPM;
        result = 1u;
    }
    else if (ROSBridge_EqualsIgnoreCase(line, "PING"))
    {
        ROSBridge_SendString("PONG\r\n");
    }
    else if (Motion_Parse(line, &command->motion, &command->target_rpm))
    {
        command->command = ROS_CMD_RUN;
        result = 1u;
    }

    if (result != 0u)
    {
        has_valid_command = 1u;
        last_valid_command_ms = Timer_GetMs();
    }

    return result;
}

static uint8_t ROSBridge_PollPort(ROS_RxContext *context, ROS_Command *command)
{
    uint8_t data;

    while (ROSBridge_ReadByte(context, &data))
    {
        if ((data == '\n') || (data == '\r'))
        {
            if ((context->line_discard == 0u) &&
                (context->line_length > 0u))
            {
                uint8_t result;
                context->line[context->line_length] = '\0';
                result = ROSBridge_ProcessLine(context->line, command);
                context->line_length = 0u;
                if (result != 0u)
                {
                    return 1u;
                }
            }
            else
            {
                context->line_length = 0u;
                context->line_discard = 0u;
            }
        }
        else if (context->line_discard == 0u)
        {
            if (context->line_length < (RX_LINE_SIZE - 1u))
            {
                context->line[context->line_length] = (char)data;
                context->line_length++;
            }
            else
            {
                context->line_length = 0u;
                context->line_discard = 1u;
            }
        }
    }

    return 0u;
}

static void ROSBridge_InitUsart1(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);

    GPIO_PinAFConfig(GPIOA, GPIO_PinSource9, GPIO_AF_USART1);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource10, GPIO_AF_USART1);

    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9 | GPIO_Pin_10;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    USART_StructInit(&USART_InitStructure);
    USART_InitStructure.USART_BaudRate = 115200u;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(USART1, &USART_InitStructure);
    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);

    NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2u;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0u;
    NVIC_Init(&NVIC_InitStructure);
    USART_Cmd(USART1, ENABLE);
}

static void ROSBridge_InitUsart3(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE);

    GPIO_PinAFConfig(GPIOD, GPIO_PinSource8, GPIO_AF_USART3);
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource9, GPIO_AF_USART3);

    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8 | GPIO_Pin_9;
    GPIO_Init(GPIOD, &GPIO_InitStructure);

    USART_StructInit(&USART_InitStructure);
    USART_InitStructure.USART_BaudRate = 115200u;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(USART3, &USART_InitStructure);
    USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);

    NVIC_InitStructure.NVIC_IRQChannel = USART3_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2u;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1u;
    NVIC_Init(&NVIC_InitStructure);
    USART_Cmd(USART3, ENABLE);
}

void ROSBridge_Init(void)
{
    uint8_t index;

    for (index = 0u; index < ROS_PORT_COUNT; index++)
    {
        rx_context[index].head = 0u;
        rx_context[index].tail = 0u;
        rx_context[index].line_length = 0u;
        rx_context[index].line_discard = 0u;
        rx_context[index].errors = 0u;
    }

    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    ROSBridge_InitUsart1();
    ROSBridge_InitUsart3();
}

uint8_t ROSBridge_PollCommand(ROS_Command *command)
{
    uint8_t port;

    for (port = 0u; port < ROS_PORT_COUNT; port++)
    {
        if (ROSBridge_PollPort(&rx_context[port], command))
        {
            return 1u;
        }
    }
    return 0u;
}

uint8_t ROSBridge_HasValidCommand(void)
{
    return has_valid_command;
}

uint32_t ROSBridge_GetCommandAgeMs(void)
{
    if (has_valid_command == 0u)
    {
        return 0u;
    }
    return Timer_GetMs() - last_valid_command_ms;
}

static uint16_t ROSBridge_AppendString(char *buffer, uint16_t position,
                                       const char *text)
{
    while (*text != '\0')
    {
        buffer[position] = *text;
        position++;
        text++;
    }
    return position;
}

static uint16_t ROSBridge_AppendUnsigned(char *buffer, uint16_t position,
                                         uint32_t value)
{
    char digits[10];
    uint8_t count = 0u;

    do
    {
        digits[count] = (char)('0' + (value % 10u));
        value /= 10u;
        count++;
    } while ((value > 0u) && (count < 10u));

    while (count > 0u)
    {
        count--;
        buffer[position] = digits[count];
        position++;
    }
    return position;
}

static uint16_t ROSBridge_AppendSigned(char *buffer, uint16_t position,
                                       int32_t value)
{
    if (value < 0)
    {
        buffer[position] = '-';
        position++;
        return ROSBridge_AppendUnsigned(buffer, position, (uint32_t)(-value));
    }
    return ROSBridge_AppendUnsigned(buffer, position, (uint32_t)value);
}

static uint16_t ROSBridge_AppendWheelValues(char *buffer, uint16_t position,
                                            const char *prefix,
                                            const int16_t values[MOTOR_COUNT])
{
    static const char wheel_name[MOTOR_COUNT] = {'A', 'B', 'C', 'D'};
    uint8_t index;

    for (index = 0u; index < MOTOR_COUNT; index++)
    {
        buffer[position] = ',';
        position++;
        position = ROSBridge_AppendString(buffer, position, prefix);
        buffer[position] = wheel_name[index];
        position++;
        buffer[position] = '=';
        position++;
        position = ROSBridge_AppendSigned(buffer, position, values[index]);
    }
    return position;
}

void ROSBridge_SendReady(void)
{
    ROSBridge_SendString("READY,PROTOCOL=OMNI_4WD_V2,DRIVE=OMNI4,"
                         "UART1+UART3=115200,HEARTBEAT_TIMEOUT_MS=500\r\n");
}

void ROSBridge_SendStatus(const ROS_Status *status)
{
    char buffer[448];
    uint16_t position = 0u;
    int32_t actual_sum = 0;
    uint8_t index;
    const char *state_text = status->running ? "RUN" : "STOP";
    const char *command_text = status->running ? "RUN" : "STOP";

    status_sequence++;
    for (index = 0u; index < MOTOR_COUNT; index++)
    {
        actual_sum += (status->actual_rpm[index] < 0) ?
                      -(int32_t)status->actual_rpm[index] : status->actual_rpm[index];
    }

    position = ROSBridge_AppendString(buffer, position, "STATUS,SEQ=");
    position = ROSBridge_AppendUnsigned(buffer, position, status_sequence);
    position = ROSBridge_AppendString(buffer, position, ",STATE=");
    position = ROSBridge_AppendString(buffer, position, state_text);
    position = ROSBridge_AppendString(buffer, position, ",CMD=");
    position = ROSBridge_AppendString(buffer, position, command_text);
    position = ROSBridge_AppendString(buffer, position, ",MOTION=");
    position = ROSBridge_AppendString(buffer, position, Motion_Name(status->motion));
    position = ROSBridge_AppendString(buffer, position, ",DRIVE=OMNI4,TARGET_RPM=");
    position = ROSBridge_AppendUnsigned(buffer, position, status->target_rpm);
    position = ROSBridge_AppendString(buffer, position, ",ACTUAL_RPM=");
    position = ROSBridge_AppendSigned(buffer, position,
                                      actual_sum / (int32_t)MOTOR_COUNT);
    position = ROSBridge_AppendWheelValues(buffer, position, "TARGET_",
                                           status->wheel_target);
    position = ROSBridge_AppendWheelValues(buffer, position, "RPM_",
                                           status->actual_rpm);
    position = ROSBridge_AppendWheelValues(buffer, position, "PWM_",
                                           status->pwm);
    position = ROSBridge_AppendString(buffer, position, ",AGE_MS=");
    position = ROSBridge_AppendUnsigned(buffer, position, status->age_ms);
    position = ROSBridge_AppendString(buffer, position, ",VALID=");
    position = ROSBridge_AppendUnsigned(buffer, position,
                                        status->command_valid);
    position = ROSBridge_AppendString(buffer, position, ",REASON=");
    position = ROSBridge_AppendString(buffer, position, status->reason);
    position = ROSBridge_AppendString(buffer, position, "\r\n");

    buffer[position] = '\0';
    ROSBridge_SendString(buffer);
}

static void ROSBridge_IrqReceive(ROS_RxContext *context, USART_TypeDef *usart)
{
    uint16_t flags = usart->SR;

    if ((flags & USART_FLAG_ORE) != 0u)
    {
        (void)usart->DR;
        context->errors++;
    }
    else if ((flags & USART_FLAG_RXNE) != 0u)
    {
        uint8_t data = (uint8_t)usart->DR;
        uint16_t next = (uint16_t)((context->head + 1u) % RX_BUFFER_SIZE);

        if (next != context->tail)
        {
            context->buffer[context->head] = data;
            context->head = next;
        }
        else
        {
            context->errors++;
        }
    }
}

void USART1_IRQHandler(void)
{
    ROSBridge_IrqReceive(&rx_context[0], USART1);
}

void USART3_IRQHandler(void)
{
    ROSBridge_IrqReceive(&rx_context[1], USART3);
}
