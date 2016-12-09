/*
 * KubOS RT
 * Copyright (C) 2016 Kubos Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "queue.h"

#include "kubos-hal/gpio.h"
#include "kubos-hal/uart.h"

#include "kubos-core/modules/fs/fs.h"
#include "kubos-core/modules/fatfs/ff.h"

#include "kubos-core/modules/fatfs/diskio.h"

#include <telemetry-aggregator/aggregator.h>
#include <telemetry/destinations.h>

#include <csp/csp.h>
#include <csp/drivers/usart.h>
#include <csp/interfaces/csp_if_kiss.h>


static inline void blink(int pin) {
    k_gpio_write(pin, 1);
    vTaskDelay(1);
    k_gpio_write(pin, 0);
}

uint16_t open_append(FIL * fp, const char * path)
{
    uint16_t result;
    result = f_open(fp, path, FA_WRITE | FA_OPEN_ALWAYS);
    if (result == FR_OK)
    {
        result = f_lseek(fp, f_size(fp));
        if (result != FR_OK)
        {
            f_close(fp);
        }
    }
    return result;
}

uint16_t write_string(char * str, int len)
{
    static FATFS FatFs;
    static FIL Fil;
    uint16_t ret;
    uint16_t bw;
    if ((ret = f_mount(&FatFs, "", 1)) == FR_OK)
    {
        if ((ret = open_append(&Fil, "data.txt")) == FR_OK)
        {
            if ((ret = f_write(&Fil, str, len, &bw)) == FR_OK)
            {
                ret = f_close(&Fil);
                blink(K_LED_GREEN);
            }
            f_close(&Fil);
        }
    }
    //f_mount(NULL, "", 0);
    return ret;
}

uint16_t open_file(FATFS * FatFs, FIL * Fil)
{
    uint16_t ret;
    if ((ret = f_mount(FatFs, "", 1)) == FR_OK)
    {
        ret = open_append(Fil, "data.txt");
    }
    //f_mount(NULL, "", 0);
    return ret;
}


uint16_t just_write(FIL * Fil, char * str, uint16_t len)
{
    uint16_t ret;
    uint16_t bw;
    if ((ret = f_write(Fil, str, len, &bw)) == FR_OK)
    {
        blink(K_LED_GREEN);
    }
    else
    {
        f_close(Fil);
    }
    //f_mount(NULL, "", 0);
    return ret;
}

void task_logging(void *p)
{
    static FATFS FatFs;
    static FIL Fil;
    char buffer[128];
    uint16_t num = 0;
    uint16_t sd_stat = FR_OK;
    uint16_t sync_count = 0;

    sd_stat = open_file(&FatFs, &Fil);
    while (1)
    {
        while ((num = k_uart_read(K_UART_CONSOLE, buffer, 128)) > 0)
        {
            if (sd_stat != FR_OK)
            {
                blink(K_LED_RED);
                f_close(&Fil);
                vTaskDelay(50);
                f_mount(NULL, "", 0);
                vTaskDelay(50);
                sd_stat = open_file(&FatFs, &Fil);
                vTaskDelay(50);
            }
            else
            {
                blink(K_LED_BLUE);
                sd_stat = just_write(&Fil, buffer, num);
                // Sync every 20 writes
                if (sd_stat == FR_OK)
                {
                    sync_count++;
                    if ((sync_count % 20) == 0)
                    {
                        sync_count = 0;
                        f_sync(&Fil);
                    }
                }
            }
        }
    }
}


void task_csp_telem_interface(void* p) {

    /* Create socket without any socket options */
    csp_socket_t *sock = csp_socket(CSP_SO_NONE);

    /* Bind all ports to socket */
    csp_bind(sock, CSP_ANY);

    /* Create 10 connections backlog queue */
    csp_listen(sock, 10);

    /* Pointer to current connection and packet */
    csp_conn_t *incoming_connection;
    csp_packet_t *packet;
    telemetry_packet* t_packet;

    /* Process incoming connections */
    while (1) {

        /* Wait for connection, 100 ms timeout */
        if ((incoming_connection = csp_accept(sock, 100)) == NULL)
            continue;

        /* Read packets. Timout is 100 ms */
        while ((packet = csp_read(incoming_connection, 100)) != NULL) {
            switch (csp_conn_dport(incoming_connection)) {
                case TELEMETRY_BEACON_PORT:
                    /* Process packet here */
                    t_packet = packet->data;
                    telemetry_submit(*t_packet);
                    blink(K_LED_GREEN);
                    csp_buffer_free(packet);
                    break;

                default:
                    csp_service_handler(incoming_connection, packet);
                    break;
            }
        }

        /* Close current connection, and handle next */
        csp_close(incoming_connection);
    }
}

/* Setup CSP interface */
static csp_iface_t csp_if_kiss;
static csp_kiss_handle_t csp_kiss_driver;

void local_usart_rx(uint8_t * buf, int len, void * pxTaskWoken) {
    csp_kiss_rx(&csp_if_kiss, buf, len, pxTaskWoken);
}


int main(void) {
    //Console is on UART2
    k_uart_console_init();

    struct usart_conf conf;
    /* set the device in KISS / UART interface */
    char dev = (char)K_UART6;
    conf.device = &dev;
    //TODO: Set this in the config.json
    conf.baudrate = 57600;
    usart_init(&conf);

    /* init kiss interface */
    csp_kiss_init(&csp_if_kiss, &csp_kiss_driver, usart_putc, usart_insert, "KISS");

    /* Setup callback from USART RX to KISS RS */
    usart_set_callback(local_usart_rx);

    /* csp buffer must be 256, or mtu in csp_iface must match */
    csp_buffer_init(5, 256);

    /* set to route through KISS / UART */
    csp_init(TELEMETRY_CSP_ADDRESS);
    csp_route_set(TELEMETRY_CSP_ADDRESS, &csp_if_kiss, CSP_NODE_MAC);
    csp_route_start_task(500, 1);


    k_gpio_init(K_LED_GREEN, K_GPIO_OUTPUT, K_GPIO_PULL_NONE);
    k_gpio_init(K_LED_ORANGE, K_GPIO_OUTPUT, K_GPIO_PULL_NONE);
    k_gpio_init(K_LED_RED, K_GPIO_OUTPUT, K_GPIO_PULL_NONE);
    k_gpio_init(K_LED_BLUE, K_GPIO_OUTPUT, K_GPIO_PULL_NONE);
    k_gpio_init(K_BUTTON_0, K_GPIO_INPUT, K_GPIO_PULL_NONE);

    //TODO: Get working along with the logging thread
    /*xTaskCreate(task_logging, "logging", configMINIMAL_STACK_SIZE * 5, NULL, 2, NULL);*/
    xTaskCreate(task_csp_telem_interface, "csp_telem_interface", configMINIMAL_STACK_SIZE * 5, NULL, 2, NULL);

    vTaskStartScheduler();

    return 0;
}
