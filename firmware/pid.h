#ifndef PID_H
#define PID_H

#include <stdbool.h>

typedef struct {
    float filtered;
    float integral;
    bool initialized;
} Pid;

float pid_step(Pid *pid, float target, float sensor, float dt);

#endif
