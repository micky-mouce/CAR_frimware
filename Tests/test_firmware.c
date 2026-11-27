#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
/* Include the real transport and state machine to exercise their static
 * functions with mocked UART/motor outputs. Hardware init is not executed. */
#include "../Hardware/ROSBridge.c"
#define main firmware_main
#include "../User/main.c"
#undef main

static uint32_t mock_ms;
static char tx[2][2048];
static unsigned int tx_length[2];
static unsigned int stops;

uint32_t Timer_GetMs(void) { return mock_ms; }
FlagStatus USART_GetFlagStatus(USART_TypeDef *uart, uint16_t flag)
{
    (void)uart; (void)flag;
    return SET;
}
void USART_SendData(USART_TypeDef *uart, uint16_t value)
{
    unsigned int port = (uart == USART1) ? 0u : 1u;
    assert(tx_length[port] + 1u < sizeof tx[port]);
    tx[port][tx_length[port]++] = (char)value;
    tx[port][tx_length[port]] = '\0';
}
void Motor_StopAll(void) { stops++; }
void Board_SetLed(uint8_t value) { (void)value; }

static uint8_t process(const char *text, ROS_Command *command)
{
    char line[128];
    assert(strlen(text) < sizeof line);
    strcpy(line, text);
    return ROSBridge_ProcessLine(line, command);
}

static void feed(unsigned int port, const char *text)
{
    ROS_RxContext *ctx = &rx_context[port];
    while (*text)
    {
        uint16_t next = (uint16_t)((ctx->head + 1u) % RX_BUFFER_SIZE);
        assert(next != ctx->tail);
        ctx->buffer[ctx->head] = (uint8_t)*text++;
        ctx->head = next;
    }
}

static void test_geometry(void)
{
    /* Recover body velocity from the four tangent projections. This checks
     * translations produce no yaw and rotation produces no translation. */
    MotionId m;
    for (m = MOTION_FORWARD; m <= MOTION_CCW; m++)
    {
        int16_t w[4];
        int forward, left, ccw;
        Motion_GetTargets(m, 40u, w);
        forward = -w[0] + w[1] - w[2] + w[3];
        left = w[0] + w[1] - w[2] - w[3];
        ccw = w[0] + w[1] + w[2] + w[3];
        assert(forward == ((m == MOTION_FORWARD) ? 160 : (m == MOTION_BACKWARD) ? -160 : 0));
        assert(left == ((m == MOTION_LEFT) ? 160 : (m == MOTION_RIGHT) ? -160 : 0));
        assert(ccw == ((m == MOTION_CCW) ? 160 : (m == MOTION_CW) ? -160 : 0));
    }
}

static void test_parser_and_watchdog(void)
{
    static const char *bad[] = {"", "R", "RU", "RUNX", "FWD,", "LEFT,-40", "CW,19",
        "CCW,81", "RUN,0", "BACK,4294967336", "RIGHT,40,1", "FWD,40junk", "STOP,40", "STATUS"};
    static const char *commands[] = {"FWD", "BACK", "LEFT", "RIGHT", "CW", "CCW"};
    ROS_Command c;
    unsigned int i;
    for (i = 0u; i < 6u; i++)
    {
        assert(process(commands[i], &c));
        assert(c.motion == (MotionId)(MOTION_FORWARD + i));
        assert(c.target_rpm == 40u);
    }
    mock_ms = 100u;
    assert(process(" \trun, 20 \t", &c));
    assert(c.command == ROS_CMD_RUN && c.motion == MOTION_FORWARD && c.target_rpm == 20u);
    assert(process("ccw,80", &c) && c.motion == MOTION_CCW && c.target_rpm == 80u);
    mock_ms = 600u;
    for (i = 0u; i < sizeof bad / sizeof bad[0]; i++) assert(!process(bad[i], &c));
    assert(!process("PING", &c));
    assert(ROSBridge_GetCommandAgeMs() == ROS_COMMAND_TIMEOUT_MS);
    assert(strcmp(tx[0], "PONG\r\n") == 0 && strcmp(tx[0], tx[1]) == 0);
    assert(process("STOP", &c) && c.command == ROS_CMD_STOP && c.motion == MOTION_STOP);
    assert(ROSBridge_GetCommandAgeMs() == 0u);
    mock_ms = UINT32_MAX - 20u;
    assert(process("LEFT", &c));
    mock_ms = 30u;
    assert(ROSBridge_GetCommandAgeMs() == 51u);

    feed(0u, "FWD,4");
    assert(!ROSBridge_PollCommand(&c));
    feed(0u, "0\r\n");
    assert(ROSBridge_PollCommand(&c) && c.motion == MOTION_FORWARD);
    feed(1u, "LEFT,30\nSTOP\n");
    assert(ROSBridge_PollCommand(&c) && c.motion == MOTION_LEFT && c.target_rpm == 30u);
    assert(ROSBridge_PollCommand(&c) && c.command == ROS_CMD_STOP);
    assert(!ROSBridge_PollCommand(&c));
    /* Overlong line must be discarded, not interpreted as a new command. */
    for (i = 0u; i < 70u; i++) feed(1u, "X");
    feed(1u, "FWD\nRIGHT,40\n");
    assert(ROSBridge_PollCommand(&c) && c.motion == MOTION_RIGHT);
}

static void test_state_and_feedback(void)
{
    unsigned int i, before;
    ROS_Status status;
    for (i = 0u; i < MOTOR_COUNT; i++) PID_Init(&wheel_pid[i]);
    Drive_ApplyTarget(MOTION_LEFT, 40u);
    Drive_EnterState(DRIVE_RUN, "ROS_RUN");
    assert(wheel_pid[0].target == 40.0f && wheel_pid[1].target == 40.0f);
    wheel_pid[0].integral = 123.0f;
    before = stops;
    Drive_ApplyTarget(MOTION_LEFT, 40u);
    assert(stops == before && wheel_pid[0].integral == 123.0f);
    Drive_ApplyTarget(MOTION_RIGHT, 40u);
    assert(stops > before && settle_ticks == 1u && wheel_pid[0].integral == 0.0f);
    Drive_EnterState(DRIVE_STOP, "TIMEOUT");
    assert(motion == MOTION_STOP && settle_ticks == 0u);
    for (i = 0u; i < MOTOR_COUNT; i++) assert(wheel_target[i] == 0 && motor_pwm[i] == 0);

    memset(&status, 0, sizeof status);
    status.running = 1u;
    status.motion = MOTION_LEFT;
    status.target_rpm = 40u;
    status.reason = "ROS_RUN";
    Motion_GetTargets(status.motion, 40u, status.wheel_target);
    for (i = 0u; i < 4u; i++) status.actual_rpm[i] = status.wheel_target[i];
    memset(tx_length, 0, sizeof tx_length);
    ROSBridge_SendStatus(&status);
    assert(strstr(tx[0], "MOTION=LEFT") && strstr(tx[0], "ACTUAL_RPM=40,"));
    assert(strstr(tx[0], "TARGET_A=40,TARGET_B=40,TARGET_C=-40,TARGET_D=-40"));
    assert(strcmp(tx[0], tx[1]) == 0);
    /* Worst-case numeric widths must fit the on-target status buffer. */
    status.age_ms = UINT32_MAX;
    status_sequence = UINT32_MAX - 1u;
    status.target_rpm = UINT16_MAX;
    status.command_valid = UINT8_MAX;
    status.motion = MOTION_RIGHT;
    status.running = 0u;
    status.reason = "HW_DISABLED";
    for (i = 0u; i < 4u; i++)
        status.wheel_target[i] = status.actual_rpm[i] = status.pwm[i] = INT16_MIN;
    memset(tx_length, 0, sizeof tx_length);
    ROSBridge_SendStatus(&status);
    assert(tx_length[0] < 448u);
}

int main(void)
{
    test_geometry();
    test_parser_and_watchdog();
    test_state_and_feedback();
    puts("PASS: omni geometry, commands, serial framing, heartbeat age, direction switch, status bounds");
    return 0;
}
