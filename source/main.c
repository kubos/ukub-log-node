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

#include <csp/csp.h>
#include <csp/arch/csp_thread.h>
#include <csp/arch/csp_time.h>
#include <csp/drivers/usart.h>
#include <csp/interfaces/csp_if_kiss.h>


#include <kubos-hal/uart.h>
#include <kubos-hal/gpio.h>

#include <telemetry/telemetry.h>
#include <telemetry/config.h>

#include "disk.h"
#include "telemetry_storage.h"
#include "misc.h"

#define FILE_NAME_BUFFER_SIZE 255

#define SENSOR_NODE_ADDRESS YOTTA_CFG_TELEMETRY_SENSOR_NODE_ADDRESS

CSP_DEFINE_TASK(telemetry_store_task) {
    /*blink(K_LED_RED);*/
    /*telemetry_packet packet = { .data.i = 0, .timestamp = 0, \*/
        /*.source.subsystem_id = 0, .source.data_type = TELEMETRY_TYPE_INT, \*/
        /*.source.source_id = 0};*/
    telemetry_packet packet;
    telemetry_conn connection = { .sources=0xFF };

    /*Subscribe to source id 0x1*/
    while (!telemetry_subscribe(&connection, 0xFF))
    {
        csp_sleep_ms(500);
    }
    printf("store_task about to read\n");
    while (1)
    {
        if (telemetry_read(connection, &packet))
        {
            print_to_console(packet);
            printf("telemetry_store packet\n");
            telemetry_store(packet);
        }
    }
}


CSP_DEFINE_TASK(task_csp_telem_interface)
{

    /*blink(K_LED_BLUE);*/
    /* Create socket without any socket options */
    csp_socket_t *sock = csp_socket(CSP_SO_NONE);

    /* Bind all ports to socket */
    int res = csp_bind(sock, 10);

    /* Create 10 connections backlog queue */
    csp_listen(sock, 10);

    /* Pointer to current connection and packet */
    csp_conn_t *incoming_connection;
    csp_packet_t *packet;
    telemetry_packet recv_packet = {.source.source_id=0xFF, .data.i=77, .source.data_type=TELEMETRY_TYPE_INT};
    printf("sending shitty packet\n");
    telemetry_publish(recv_packet);
    printf("sent shitty packet\n");

    /* Process incoming connections */
    while (1)
    {

        /* Wait for connection, 100 ms timeout */
        if ((incoming_connection = csp_accept(sock, 100)) == NULL)
            continue;

        /* Read packets. Timout is 100 ms */
        while ((packet = csp_read(incoming_connection, 100)) != NULL)
        {
            switch (csp_conn_dport(incoming_connection))
            {
                case 10:
                    /* Process packet here */
                    memcpy(&recv_packet, packet->data, sizeof(telemetry_packet));
                    telemetry_publish(recv_packet);
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

void local_usart_rx(uint8_t * buf, int len, void * pxTaskWoken)
{
    csp_kiss_rx(&csp_if_kiss, buf, len, pxTaskWoken);
}

int main(void)
{
    //Console is on UART2
    k_uart_console_init();

    struct usart_conf conf;
    /* set the device in KISS / UART interface */
    char dev = (char)K_UART2;
    conf.device = &dev;
    conf.baudrate = K_UART_CONSOLE_BAUDRATE;
    usart_init(&conf);

    /* init kiss interface */
    csp_kiss_init(&csp_if_kiss, &csp_kiss_driver, usart_putc, usart_insert, "KISS");

    /* Setup callback from USART RX to KISS RS */
    usart_set_callback(local_usart_rx);

    /* set to route through KISS / UART */
    telemetry_init();
    /*csp_init(TELEMETRY_CSP_ADDRESS);*/
    csp_route_set(1, &csp_if_kiss, CSP_NODE_MAC);
    /*csp_route_start_task(1000, 1);*/

    k_gpio_init(K_LED_GREEN, K_GPIO_OUTPUT, K_GPIO_PULL_NONE);
    k_gpio_init(K_LED_ORANGE, K_GPIO_OUTPUT, K_GPIO_PULL_NONE);
    k_gpio_init(K_LED_RED, K_GPIO_OUTPUT, K_GPIO_PULL_NONE);
    k_gpio_init(K_LED_BLUE, K_GPIO_OUTPUT, K_GPIO_PULL_NONE);

    //TODO: Get working along with the logging thread
    /*xTaskCreate(task_logging, "logging", configMINIMAL_STACK_SIZE * 5, NULL, 2, NULL);*/
    csp_thread_create(task_csp_telem_interface, "CSP_INT", 1000, NULL, 0, NULL);


    /* Logging task for telemetry packets */
    csp_thread_handle_t handle_telemetry_store_task;
    csp_thread_create(telemetry_store_task, "SUB", 1000, NULL, 0, &handle_telemetry_store_task);

    vTaskStartScheduler();
    while(1);

    return 0;
}
