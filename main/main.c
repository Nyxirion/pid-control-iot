#include <stdio.h>
#include "driver/gpio.h"
#include "esp_timer.h"
#include "driver/ledc.h"
#include "pid-controller.h"

//pid functions//

esp_err_t pid_init(void){
    pid_controller_t pid_level = {
    .kp = 1,
    .ki = 0.5,
    .kd = 0.2,

    .setpoint = 20,
    .sampleTime = 1,
    .filterTau = 5,
    .antiWindupGain = 0.8,

    .maxOutputLim = 255,
    .minOutputLim = 0,
    };
    pid_config_init(&pid_level);
    return ESP_OK;
}

//PWM functions//
esp_err_t setPWM(void){
    //TIMER CONFIG
    ledc_timer_config_t timer_cfg = {0};
    timer_cfg.speed_mode = LEDC_HIGH_SPEED_MODE;
    timer_cfg.duty_resolution = LEDC_TIMER_8_BIT;
    timer_cfg.timer_num = LEDC_TIMER_0;
    timer_cfg.freq_hz = 2000;

    ledc_channel_config_t channel_cfg ={0};
    channel_cfg.gpio_num = 25;
    channel_cfg.speed_mode = LEDC_HIGH_SPEED_MODE;
    channel_cfg.channel = LEDC_CHANNEL_0;
    channel_cfg.timer_sel = LEDC_TIMER_0;
    channel_cfg.duty = 0;

    ledc_timer_config(&timer_cfg);
    ledc_channel_config(&channel_cfg);


    return ESP_OK;

}

esp_err_t get_distance()
void pid_task(void *pvParameter){

}
void app_main(void)
{
    pid_init();
    setPWM();

}