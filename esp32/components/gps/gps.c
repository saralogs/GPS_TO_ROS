#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "gps.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_log.h"

#ifndef CONFIG_LOG_MAXIMUM_LEVEL
#define CONFIG_LOG_MAXIMUM_LEVEL ESP_LOG_DEBUG
#endif

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

// GPS fix structure
typedef struct {
    double lat;
    double lon;
    float speed;
    float altitude;
    int satellites;
    bool valid;          
} gps_fix_t;

static gps_fix_t gps_fix = {0};

static bool rmc_updated = false;
static bool gga_updated = false;

// Validate NMEA sentence checksum (XOR of all chars between '$' and '*')
static bool validate_checksum(const char *sentence) {
    const char *p = sentence;
    if (*p != '$') return false;

    uint8_t calc = 0;
    p++;  //skip '$'
    while (*p && *p != '*') {
        calc ^= *p;
        p++;
    }
    if (*p != '*') return false;

    //read the two hex digits after '*' 
    char hex[3] = {p[1], p[2], '\0'};
    char *endptr;
    long checksum = strtol(hex, &endptr, 16);
    if (endptr != hex + 2) return false; 

    return (uint8_t)checksum == calc;
}

static void publish_fix(void);
static void handle_rmc(const char *sentence);
static void handle_gga(const char *sentence);

//parse a complete NMEA sentence, dispatching to handlers based on type
static void parse_nmea(const char *sentence)
{
    if (!validate_checksum(sentence)) {
        ESP_LOGD(TAG, "Checksum failed, ignoring sentence");
        return;
    }

    enum minmea_sentence_id id;
    id = minmea_sentence_id(sentence, false);

    switch (id) {
        case MINMEA_SENTENCE_RMC:
            handle_rmc(sentence);
            break;
        case MINMEA_SENTENCE_GGA:
            handle_gga(sentence);
            break;
        default:
            //ignore other sentence types
            break;
    }
}

//Handle RMC sentence: update lat, lon, speed, and valid flag. If both RMC and GGA have updated, publish the fix.
static void handle_rmc(const char *sentence)
{
    struct minmea_sentence_rmc rmc;
    if (!minmea_parse_rmc(&rmc, sentence))
        return;

    gps_fix.valid = rmc.valid;

    if (gps_fix.valid) {
        gps_fix.lat = minmea_tocoord(&rmc.latitude);
        gps_fix.lon = minmea_tocoord(&rmc.longitude);
        gps_fix.speed = minmea_tofloat(&rmc.speed);

        rmc_updated = true;
    }

    if (rmc_updated && gga_updated) {
        publish_fix();
        rmc_updated = false;
        gga_updated = false;
    }
}

// Handle GGA sentence: update altitude and satellite count. 
static void handle_gga(const char *sentence)
{
    struct minmea_sentence_gga gga;
    if (!minmea_parse_gga(&gga, sentence))
        return;

    gps_fix.altitude = minmea_tofloat(&gga.altitude);
    gps_fix.satellites = gga.satellites_tracked;

    gga_updated = true;

    if (rmc_updated && gga_updated) {
        publish_fix();
        rmc_updated = false;
        gga_updated = false;
    }
}

static void publish_fix(void)
{
    if (!gps_fix.valid)
        return;

    char buffer[128];

    snprintf(buffer, sizeof(buffer),
             "$GPS,%.6f,%.6f,%.2f,%d,%.2f\n",
             gps_fix.lat,
             gps_fix.lon,
             gps_fix.altitude,
             gps_fix.satellites,
             gps_fix.speed);

    uart_write_bytes(ROS_UART, buffer, strlen(buffer));
}

// Build sentences from UART stream, resetting on '$' and processing on newline.
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

    uart_driver_install(UART_PORT, BUF_SIZE * 2, 0, 0, NULL, 0);
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

            // Sentence start: reset line buffer and begin new sentence
            if (c == '$') {
                line_idx = 0;
                line[line_idx++] = c;  /* store the '$' */
            }

            // If we are already inside a sentence (line_idx > 0), accumulate 
            else if (line_idx > 0) {
                if (c == '\n') {
                    line[line_idx] = '\0';     //terminate
                    parse_nmea(line);           //process complete sentence
                    line_idx = 0;                //ready for next
                } else if (line_idx < LINE_SIZE - 1) {
                    line[line_idx++] = c;
                } else {
                    // line too long: discard and reset
                    ESP_LOGW(TAG, "Line too long, discarding");
                    line_idx = 0;
                }
            }
        }
    }
}

void gps_init(void)
{
    xTaskCreate(gps_task, "gps_task", 4096, NULL, 10, NULL);
}       
