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
#include <csp/drivers/usart.h>
#include <csp/interfaces/csp_if_kiss.h>

#include <kubos-hal/uart.h>
#include <kubos-hal/gpio.h>

#include <telemetry/telemetry.h>

#include "disk.h"
#include "telemetry_storage.h"
#include "misc.h"


#define SENSOR_NODE_ADDRESS YOTTA_CFG_CSP_SENSOR_NODE_ADDRESS
#define SENSOR_NODE_PORT YOTTA_CFG_CSP_SENSOR_NODE_PORT
#define CSP_UART_BAUDRATE YOTTA_CFG_CSP_BAUDRATE
#define CSP_UART_BUS YOTTA_CFG_CSP_UART_BUS
#define CSP_MY_ADDRESS YOTTA_CFG_CSP_MY_ADDRESS
#define CSP_MY_PORT YOTTA_CFG_CSP_PORT


CSP_DEFINE_TASK(telemetry_store_task) {

    telemetry_packet packet;
    telemetry_conn connection;

    /* Subscribe to all telemetry publishers */
    while (!telemetry_subscribe(&connection, 0x0))
    {
        /* Retry subscribing every 5 ms */
        csp_sleep_ms(5);
    }

    while (1)
    {
        if (telemetry_read(connection, &packet))
        {
            /* Store telemetry packets from the telemetry system */
            telemetry_store(packet);
            blink(K_LED_RED);
        }
    }
}


CSP_DEFINE_TASK(uart_rx_task)
{
    /* Create socket without any socket options */
    csp_socket_t *sock = csp_socket(CSP_SO_NONE);

    /* Bind the configured port to the socket */
    csp_bind(sock, CSP_MY_PORT);

    /* Create 10 connections backlog queue */
    csp_listen(sock, 10);

    /* Pointer to current connection and packet */
    csp_conn_t *incoming_connection;
    csp_packet_t *packet;

    telemetry_packet recv_packet;

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
                case SENSOR_NODE_PORT:
                    /* Copy the telemetry packet data from the received csp packet */
                    memcpy(&recv_packet, packet->data, sizeof(telemetry_packet));
                    /* Submit the telemetry packet into the telemetry system */
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
    k_uart_console_init();
    
    k_gpio_init(K_LED_GREEN, K_GPIO_OUTPUT, K_GPIO_PULL_NONE);
    k_gpio_init(K_LED_ORANGE, K_GPIO_OUTPUT, K_GPIO_PULL_NONE);
    k_gpio_init(K_LED_RED, K_GPIO_OUTPUT, K_GPIO_PULL_NONE);
    k_gpio_init(K_LED_BLUE, K_GPIO_OUTPUT, K_GPIO_PULL_NONE);

    /* Configure and initialize UART bus and baudrate */
    struct usart_conf conf;
    char dev = (char)CSP_UART_BUS;
    conf.device = &dev;
    conf.baudrate = CSP_UART_BAUDRATE;
    usart_init(&conf);

    /* Initialize KISS interface */
    csp_kiss_init(&csp_if_kiss, &csp_kiss_driver, usart_putc, usart_insert, "KISS");

    /* Setup callback from USART RX to KISS RS */
    usart_set_callback(local_usart_rx);

    /* Initialize the telemetry_system */
    telemetry_init();
    
    /* Set to route through KISS / UART */
    csp_route_set(SENSOR_NODE_ADDRESS, &csp_if_kiss, CSP_NODE_MAC);

    /* Receiving task for CSP UART */
    csp_thread_handle_t handle_uart_rx_task;
    csp_thread_create(uart_rx_task, "RX", 1000, NULL, 1, &handle_uart_rx_task);

    /* Logging task for telemetry packets */
    csp_thread_handle_t handle_telemetry_store_task;
    csp_thread_create(telemetry_store_task, "SUB", 1000, NULL, 1, &handle_telemetry_store_task);

    vTaskStartScheduler();
    
    while(1);

    return 0;
}
