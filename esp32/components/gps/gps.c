#include <string.h>
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_log.h"

#include "minmea.h"

#define UART_PORT UART_NUM_1
#define ROS_UART UART_NUM_0
#define TXD 17
#define RXD 16
#define UART_BAUD 9600

#define BUF_SIZE 256
#define LINE_SIZE 128

static char line[LINE_SIZE];
static int line_idx = 0;

static const char *TAG = "GPS";

void send_gps_data(float lat, float lon, float speed) {
    char buffer[128];
    snprintf(buffer, sizeof(buffer), 
             "GPS_DATA:%.6f,%.6f,%.2f\n", lat, lon, speed);
    uart_write_bytes(ROS_UART, buffer, strlen(buffer));
}

static void parse_nmea(const char *sentence)
{
    struct minmea_sentence_rmc rmc;

    if (minmea_sentence_id(sentence, false) == MINMEA_SENTENCE_RMC) {
        if (minmea_parse_rmc(&rmc, sentence)) {

            if (rmc.valid) {
                double lat = minmea_tocoord(&rmc.latitude);
                double lon = minmea_tocoord(&rmc.longitude);

                ESP_LOGI(TAG, "Lat: %.6f, Lon: %.6f", lat, lon);
            }
            if (rmc.valid) {
                double lat = minmea_tocoord(&rmc.latitude);
                double lon = minmea_tocoord(&rmc.longitude);
                float speed = minmea_tofloat(&rmc.speed);
    
                send_gps_data(lat, lon, speed);
            }
        }
    }
}

static void gps_task(void *arg)
{
    ESP_LOGI(TAG, "GPS task started");

    uart_config_t cfg = {
        .baud_rate = UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };

    uart_driver_install(UART_PORT, BUF_SIZE, 0, 0, NULL, 0);
    uart_param_config(UART_PORT, &cfg);
    uart_set_pin(UART_PORT, TXD, RXD,
                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    uint8_t data[BUF_SIZE];

    while (1) {
        int len = uart_read_bytes(
            UART_PORT, data, BUF_SIZE, 20 / portTICK_PERIOD_MS
        );

        for (int i = 0; i < len; i++) {
            char c = data[i];

            if (c == '\n') {
                line[line_idx] = '\0';
                parse_nmea(line);
                line_idx = 0;
            } else if (line_idx < LINE_SIZE - 1) {
                line[line_idx++] = c;
            }
        }
    }
}

void gps_init(void)
{
    xTaskCreate(gps_task, "gps_task", 4096, NULL, 10, NULL);
}
