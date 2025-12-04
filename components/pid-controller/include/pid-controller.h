#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef struct
{
    //basic parameters
    float kp, ki, kd;

    //controller "memory" 
    float integral;
    float prevError;
    float derivative;

    //config
    float setpoint;
    float sampleTime;
    float filterTau;
    float antiWindupGain;

    //limits 
    float maxOutputLim;
    float minOutputLim;

    //output
    float output;
    bool useAntiWindup;

} pid_controller_t;
