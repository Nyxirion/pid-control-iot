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
    float sampleTime; // in seconds
    float filterTau;
    float antiWindupGain;

    //limits 
    float maxOutputLim;
    float minOutputLim;

    //output
    float output;
    bool useAntiWindup;

} pid_controller_t;

void pid_config_init(pid_controller_t *pid);
float computePID(pid_controller_t *pid, float measurement);
