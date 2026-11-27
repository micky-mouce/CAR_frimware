#include "Motion.h"

static uint8_t Motion_TokenEquals(const char *text, uint16_t length,
                                  const char *expected)
{
    uint16_t i;
    for (i = 0u; i < length; i++)
    {
        char c = text[i];
        if (expected[i] == '\0') return 0u;
        if ((c >= 'a') && (c <= 'z')) c = (char)(c - 'a' + 'A');
        if (c != expected[i]) return 0u;
    }
    return (uint8_t)(expected[length] == '\0');
}

uint8_t Motion_Parse(const char *line, MotionId *motion, uint16_t *rpm)
{
    const char *token;
    const char *end;
    uint16_t length;
    uint32_t value = MOTION_DEFAULT_RPM;
    MotionId selected;

    while ((*line == ' ') || (*line == '\t')) line++;
    token = line;
    while ((*line != '\0') && (*line != ',') &&
           (*line != ' ') && (*line != '\t')) line++;
    length = (uint16_t)(line - token);
    if (Motion_TokenEquals(token, length, "FWD") ||
        Motion_TokenEquals(token, length, "RUN")) selected = MOTION_FORWARD;
    else if (Motion_TokenEquals(token, length, "BACK")) selected = MOTION_BACKWARD;
    else if (Motion_TokenEquals(token, length, "LEFT")) selected = MOTION_LEFT;
    else if (Motion_TokenEquals(token, length, "RIGHT")) selected = MOTION_RIGHT;
    else if (Motion_TokenEquals(token, length, "CW")) selected = MOTION_CW;
    else if (Motion_TokenEquals(token, length, "CCW")) selected = MOTION_CCW;
    else return 0u;

    while ((*line == ' ') || (*line == '\t')) line++;
    if (*line == ',')
    {
        line++;
        while ((*line == ' ') || (*line == '\t')) line++;
        end = line;
        value = 0u;
        while ((*line >= '0') && (*line <= '9'))
        {
            value = value * 10u + (uint32_t)(*line - '0');
            if (value > MOTION_MAX_RPM) return 0u;
            line++;
        }
        if ((line == end) || (value < MOTION_MIN_RPM)) return 0u;
        while ((*line == ' ') || (*line == '\t')) line++;
    }
    if (*line != '\0') return 0u;
    *motion = selected;
    *rpm = (uint16_t)value;
    return 1u;
}

void Motion_GetTargets(MotionId motion, uint16_t rpm, int16_t targets[4])
{
    /* Rows STOP/FWD/BACK/LEFT/RIGHT/CW/CCW; columns A/B/C/D.
     * In body axes x=front, y=left, positive wheel tangent vectors are:
     * A=(-x,+y), B=(+x,+y), C=(-x,-y), D=(+x,-y).
     * All positive wheel speeds turn the chassis CCW. Translation projects
     * body velocity onto these tangents; front bisects wheels A and B.
     * Common sqrt(1/2) factors are absorbed into wheel RPM magnitude.
     */
    static const int8_t sign[7][4] =
    {
        { 0, 0, 0, 0},
        {-1, 1,-1, 1},
        { 1,-1, 1,-1},
        { 1, 1,-1,-1},
        {-1,-1, 1, 1},
        {-1,-1,-1,-1},
        { 1, 1, 1, 1}
    };
    uint8_t i;
    if ((unsigned int)motion > (unsigned int)MOTION_CCW) motion = MOTION_STOP;
    if (rpm > MOTION_MAX_RPM) rpm = MOTION_MAX_RPM;
    for (i = 0u; i < 4u; i++) targets[i] = (int16_t)(sign[motion][i] * (int16_t)rpm);
}

const char *Motion_Name(MotionId motion)
{
    static const char * const names[7] = {"STOP", "FWD", "BACK", "LEFT", "RIGHT", "CW", "CCW"};
    if ((unsigned int)motion > (unsigned int)MOTION_CCW) return "STOP";
    return names[motion];
}
