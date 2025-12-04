#include <stdio.h>
#include "pid-controller.h"

float computePID(pid_controller_t *pid, float measurement){
    float error = pid->setpoint - measurement;
    float tentativeOutput; 
    float windup_compensation = 0.0f;
    float P = pid->kp * error;

    pid->integral = pid->integral + pid->ki * pid->sampleTime * 0.5f* (error + pid->prevError); 

    pid->derivative = 2.0f*pid->kd*(error - pid->prevError)/(pid->sampleTime + 2.0f* pid->filterTau)
                    + (2.0f*pid->filterTau - pid->sampleTime) * pid->derivative 
                    / (2.0f*pid->filterTau + pid->sampleTime);

    tentativeOutput = P + pid->integral + pid->derivative;

    //we check is the tentative output is saturated and apply limits 

    if(tentativeOutput > pid->maxOutputLim){
        pid->output = pid->maxOutputLim;
        windup_compensation = pid->maxOutputLim - tentativeOutput;
    }

    else if(tentativeOutput < pid->minOutputLim){
        pid->output = pid->minOutputLim;
        windup_compensation = pid->minOutputLim -tentativeOutput;
    }
    else{
        pid->output = tentativeOutput;
    }

    if(pid->useAntiWindup){
        // Asegurar que pid->antiWindupGain esté entre 0 y 1 (típicamente 0.1 a 1.0)
        pid->integral = pid->integral + pid->antiWindupGain * windup_compensation;
    }   


    pid->prevError = error;

    return pid->output;
}