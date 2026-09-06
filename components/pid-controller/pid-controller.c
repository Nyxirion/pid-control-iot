#include "pid-controller.h"

void pid_config_init(pid_controller_t* pid){
    pid->integral = 0.0f;
    pid->prevError = 0.0f;
    pid->derivative = 0.0f;
    pid->output = 0.0f;
    pid->useAntiWindup = true; // this is set true by default for security reasons 
}

float computePID(pid_controller_t *pid, float measurement){
    float error = pid->setpoint - measurement;
    float tentativeOutput; 

    float P = pid->kp * error;

    float integralTerm = pid->integral + pid->ki * pid->sampleTime * 0.5f* (error + pid->prevError); 

    pid->derivative = 2.0f*pid->kd*(error - pid->prevError)/(pid->sampleTime + 2.0f* pid->filterTau)
                    + (2.0f*pid->filterTau - pid->sampleTime) * pid->derivative 
                    / (2.0f*pid->filterTau + pid->sampleTime);

    tentativeOutput = P + pid->integral + pid->derivative;

    //we check is the tentative output is saturated and apply limits 

    if(tentativeOutput > pid->maxOutputLim){
        pid->output = pid->maxOutputLim;

        if(pid->useAntiWindup){
            // No actualizar la integral cuando está saturada
            // La integral permanece como estaba 
            // No hacer nada con pid -> integral
        }
        
        else{
            pid->integral = integralTerm;
        }


    }
    else if(tentativeOutput < pid->minOutputLim){
        pid->output = pid->minOutputLim;

        // CLAMPING: solo actualizar la integral si NO está saturado
        if(pid->useAntiWindup){
            // No actualizar la integral cuando está saturada
            // La integral permanece como estaba 
        }
        else{
            pid->integral = integralTerm;
        }
    }

    else{
        pid->output = tentativeOutput;
        // Si no está saturado, actualizar la integral normalmente
        pid->integral = integralTerm;
    }


    pid->prevError = error;

    return pid->output;
}