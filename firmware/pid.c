#include "pid.h"

static float clamp(float value, float min, float max)
{
    if (value < min) {
        return min;
    }
    return value > max ? max : value;
}

float pid_step(Pid *pid, float target, float sensor, float dt)
{
    const float kp = 1.35f;
    const float ki = 3.0f;
    const float kd = 0.015f;
    const float filter_tau = 0.040f;

    if (!pid->initialized) {
        pid->filtered = sensor;
        pid->initialized = true;
    }
    float previous = pid->filtered;
    pid->filtered += dt / (filter_tau + dt) * (sensor - pid->filtered);
    float error = target - pid->filtered;
    float pd = kp * error - kd * (pid->filtered - previous) / dt;
    float integral = clamp(pid->integral + ki * error * dt, 0.0f, 1.0f);
    float candidate = pd + integral;

    /* Stop accumulating only when the error drives the output farther into a limit. */
    if ((candidate <= 1.0f || error < 0.0f) &&
        (candidate >= 0.0f || error > 0.0f)) {
        pid->integral = integral;
    }
    return clamp(pd + pid->integral, 0.0f, 1.0f);
}
