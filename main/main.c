#include <stdio.h>
#include <string.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "driver/ledc.h"
#include "pid-controller.h"
#include "ultrasonic.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

// defines para el pid
#define MAX_DISTANCE_CM 500
#define H_MAX 36.05 // Altura máxima del tanque
#define TRIGGER_GPIO 17
#define ECHO_GPIO 16
#define STACK_SIZE 4096

// defines para el wifi
#define ESP_WIFI_SSID "ESP32 tank controller"
#define ESP_WIFI_PASS "tank-controller-1234"
#define ESP_WIFI_CHANNEL 1
#define MAX_STA_CONN 2

// wifi event handler//
static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    printf("event nr: %ld!\n", event_id);
}
// function to initialize wifi//
void wifi_init_softap()
{
    // this initialize the IP stack
    esp_event_loop_create_default();
    esp_netif_init();
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_instance_register(WIFI_EVENT,
                                        ESP_EVENT_ANY_ID,
                                        &wifi_event_handler,
                                        NULL,
                                        NULL);
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = ESP_WIFI_SSID,
            .ssid_len = strlen(ESP_WIFI_SSID),
            .channel = ESP_WIFI_CHANNEL,
            .password = ESP_WIFI_PASS,
            .max_connection = MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .required = true,
            }}};
    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
    esp_wifi_start();
}
// pid object//
pid_controller_t pid_level = {
    .kp = 20,
    .ki = 5,
    .kd = 1,

    .setpoint = 15,
    .sampleTime = 1,
    .filterTau = 5,
    .antiWindupGain = 0.8,

    .maxOutputLim = 255,
    .minOutputLim = 0,
};
ultrasonic_sensor_t sonic_sensor = {
    .trigger_pin = TRIGGER_GPIO,
    .echo_pin = ECHO_GPIO};

void pid_task(void *pvParameter)
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xPeriod = pdMS_TO_TICKS(500);

    float distance;
    while (1)
    {
        TickType_t xStartTime = xTaskGetTickCount();

        esp_err_t res = ultrasonic_measure(&sonic_sensor, MAX_DISTANCE_CM, &distance);
        if (res != ESP_OK)
        {
            printf("Error %d: ", res);
            switch (res)
            {
            case ESP_ERR_ULTRASONIC_PING:
                printf("Cannot ping (device is in invalid state)\n");
                break;
            case ESP_ERR_ULTRASONIC_PING_TIMEOUT:
                printf("Ping timeout (no device found)\n");
                break;
            case ESP_ERR_ULTRASONIC_ECHO_TIMEOUT:
                printf("Echo timeout (i.e. distance too big)\n");
                break;
            default:
                printf("%s\n", esp_err_to_name(res));
            }
        }
        else
        {
            distance = H_MAX - distance * 100;
            printf("Distance: %0.02f cm\n", distance);

            uint32_t output = (int)computePID(&pid_level, distance);
            ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, output);
            ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0);

            // medir tiempo de ejecucion
            TickType_t xExecutionTime = xTaskGetTickCount() - xStartTime;
            // Para saber, en caso de que alguna vez se llegue a pasar
            if (xExecutionTime >= xPeriod)
            {
                printf("¡ADVERTENCIA! Tiempo excedido: %lu ms (período: %lu ms)\n",
                       xExecutionTime * portTICK_PERIOD_MS, xPeriod * portTICK_PERIOD_MS);
            }
            vTaskDelayUntil(&xLastWakeTime, xPeriod);
        }
    }
}

// PWM functions//
esp_err_t setPWM(void)
{
    // TIMER CONFIG
    ledc_timer_config_t timer_cfg = {0};
    timer_cfg.speed_mode = LEDC_HIGH_SPEED_MODE;
    timer_cfg.duty_resolution = LEDC_TIMER_8_BIT;
    timer_cfg.timer_num = LEDC_TIMER_0;
    timer_cfg.freq_hz = 2000;

    ledc_channel_config_t channel_cfg = {0};
    channel_cfg.gpio_num = 25;
    channel_cfg.speed_mode = LEDC_HIGH_SPEED_MODE;
    channel_cfg.channel = LEDC_CHANNEL_0;
    channel_cfg.timer_sel = LEDC_TIMER_0;
    channel_cfg.duty = 0;

    ledc_timer_config(&timer_cfg);
    ledc_channel_config(&channel_cfg);

    return ESP_OK;
}

// initialize task//

esp_err_t create_task(void)
{
    static uint8_t ucParameterToPass;
    TaskHandle_t xHandle = NULL;

    xTaskCreatePinnedToCore(pid_task,
                            "TareaPID",
                            STACK_SIZE,
                            &ucParameterToPass,
                            1, // Priority
                            &xHandle,
                            1); // core number
    return ESP_OK;
}

void app_main(void)
{
    //initialize NVS for wifi
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_init_softap();
    ultrasonic_init(&sonic_sensor);
    setPWM();
    pid_config_init(&pid_level);
    create_task();
}