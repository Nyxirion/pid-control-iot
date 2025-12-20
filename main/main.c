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
#include "esp_http_server.h"
#include "esp_log.h"
#include "freertos/queue.h"

//Declarar cola para la comunicacion entre tareas 
QueueHandle_t pid_to_broadcast_queue = 0;

//defines para el LOG 
static const char* TAG = "Server";

//defines para el websocket server 
//para el server
httpd_handle_t server = NULL;
char index_html[] = "<!DOCTYPE html><html><head><title>Page Title</title></head><body style='background-color: #EEEEEE;'><span style='color: #003366;'><h1>Lets generate a random number</h1><p>The random number is: <span id='rand'>-</span></p><p><button type='button' id='BTN_SEND_BACK'>Send info to ESP32</button></p></span></body><script> var Socket; document.getElementById('BTN_SEND_BACK').addEventListener('click', button_send_back); function init() { let gateway = 'http://192.168.4.1/ws'; Socket = new WebSocket(gateway); Socket.onmessage = function(event) { processCommand(event); }; } function button_send_back() { Socket.send('toggle'); } function processCommand(event) { const data = JSON.parse(event.data); console.log('Nivel actual:',data.measurement); console.log('Output PID:', data.output); document.getElementById('rand').innerHTML = event.data; console.log(event.data); } window.onload = function(event) { init(); }</script></html>";
char buffer[4096];


//para el ws handler 
int client_fds[5];
int num_clients = 0;


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
//function to send data to all clients 
void send_to_all_clients(const char* data) // funcion que manda los datos a todos los clientes 
{
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = (uint8_t*)data;
    ws_pkt.len = strlen(data);
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    
    for (int i = 0; i < num_clients; i++) {
        if (client_fds[i] != -1) {
            httpd_ws_send_frame_async(server, client_fds[i], &ws_pkt);
        }
    }
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
    pid_broadcast_data_t pid_data = {0};
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
            //printf("Distance: %0.02f cm\n", distance);

            uint32_t output = (int)computePID(&pid_level, distance);
            ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, output);
            ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0);

            //hasta aquí termina el PID

            //empaquetado de datos en la estructura
            printf("distance is %.2f\n", distance);
            pid_data.measurement = distance;
            pid_data.output = pid_level.output;

            if(!xQueueSend(pid_to_broadcast_queue, &pid_data, 0)){
                printf("Error Sending to queue\n");
            }
            //
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

void broadcast_task(void *pvParameter)
{
    while(1){
        pid_broadcast_data_t received;

        if(!xQueueReceive(pid_to_broadcast_queue, &received, pdMS_TO_TICKS(2000))){
            printf("ERROR recibiendo dato en la cola");
        }
    else{
        char json[128];
        snprintf(json, sizeof(json), "{\"measurement\":%f, \"output\":%f}", received.measurement, received.output);
        send_to_all_clients(json);
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

    xTaskCreatePinnedToCore(broadcast_task,
                            "TareaTRANSMISION",
                            STACK_SIZE,
                            &ucParameterToPass,
                            5, // Priority
                            &xHandle,
                            tskNO_AFFINITY); // core number                            
    return ESP_OK;
}

//creating ws connection 

//defining the handlers to the URI handlers, this is equivalent to say what are we gonna do is the client request those resources
esp_err_t ws_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET)
    {
        // Registrar nuevo cliente
        int sockfd = httpd_req_to_sockfd(req);
        client_fds[num_clients++] = sockfd;
        ESP_LOGI(TAG, "Nuevo cliente conectado. Total: %d", num_clients);
        return ESP_OK;
    }

    httpd_ws_frame_t ws_pkt;  //se crea un paquete o frame donde se va a guardar los datos recibidos

    uint8_t *buf = NULL; //buffer donde se almacenara la data antes de pasarla a ws_pkt

    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t)); // se limpia ws_pkt y se pone todo a 0 

    ws_pkt.type = HTTPD_WS_TYPE_TEXT; //tipo de dato de ws pkt

    /* Set max_len = 0 to get the frame len */
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "httpd_ws_recv_frame failed to get frame len with %d", ret);
        return ret;
    }

    ESP_LOGI(TAG, "frame len is %d", ws_pkt.len);    
    if (ws_pkt.len) { 
        /* ws_pkt.len + 1 is for NULL termination as we are expecting a string */
        buf = calloc(1, ws_pkt.len + 1); // Crea en el buffer 1 elemento con el numero de bytes necesarios
        if (buf == NULL) {
            ESP_LOGE(TAG, "Failed to calloc memory for buf");
            return ESP_ERR_NO_MEM;
        }
        ws_pkt.payload = buf; //payload tiene que apuntar a esta direccion en memoria donde quiero que escriba

        /* Set max_len = ws_pkt.len to get the frame payload */

        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);  //finalmente obtenemos los datos
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "httpd_ws_recv_frame failed with %d", ret);
            free(buf);
            return ret;
        }
        ESP_LOGI(TAG, "Got packet with message: %s", ws_pkt.payload);
    }

        //vienen las decisiones, if datos == APAGAR, entonces apaga
    ESP_LOGI(TAG, "Packet type: %d", ws_pkt.type);
    if (ws_pkt.type == HTTPD_WS_TYPE_TEXT &&
        strcmp((char*)ws_pkt.payload,"tfff") == 0) {
        free(buf);
        return printf("Chavez"); //funcion que dispara respuesta asincrona
    }

    ret = httpd_ws_send_frame(req, &ws_pkt);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "httpd_ws_send_frame failed with %d", ret);
    }
    free(buf);
    return ret;
}

esp_err_t get_webpage(httpd_req_t *req)
{
    int response;
    snprintf(buffer, sizeof(buffer), "%s", index_html);
    response = httpd_resp_send(req, buffer, HTTPD_RESP_USE_STRLEN);
    return response;
}

// defining the URI's
static const httpd_uri_t ws = {
        .uri        = "/ws",
        .method     = HTTP_GET,
        .handler    = ws_handler,
        .user_ctx   = NULL,
        .is_websocket = true
};
static const httpd_uri_t wp = {
        .uri        = "/",
        .method     = HTTP_GET,
        .handler    = get_webpage,
        .user_ctx   = NULL,
};

static httpd_handle_t start_webserver(void) //this function initialize the server and its in charge to register the URI's
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Registering the ws handler
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &wp);
        httpd_register_uri_handler(server, &ws);
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
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
    //defining queues 
    pid_to_broadcast_queue = xQueueCreate(10, sizeof(pid_broadcast_data_t));
    // //
    pid_config_init(&pid_level);
    create_task();
    server = start_webserver();
}