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

#include "kubos-hal/uart.h"
#include "kubos-hal/gpio.h"

#include <telemetry/telemetry.h>
#include <telemetry-aggregator/aggregator.h>
#include <telemetry/destinations.h>

#include "telemetry_storage.h"
#include "misc.h"

#define FILE_NAME_BUFFER_SIZE 255
#define DATA_BUFFER_SIZE 128


CSP_DEFINE_TASK(task_server) {

    csp_socket_t *sock = csp_socket(CSP_SO_NONE);
    csp_bind(sock, CSP_ANY);
    csp_listen(sock, 10);

    csp_conn_t *conn;
    csp_packet_t *packet;
    telemetry_packet data;
    telemetry_packet *data_ptr;

    while (1) {

        if ((conn = csp_accept(sock, 10000)) == NULL)
            continue;

        while ((packet = csp_read(conn, 100)) != NULL) {
            switch (csp_conn_dport(conn)) {
            case TELEMETRY_BEACON_PORT:
                data = *((telemetry_packet*)packet->data);
                telemetry_store(data, TELEMETRY_CSP_ADDRESS);
                csp_buffer_free(packet);
                #if defined(TARGET_LIKE_FREERTOS) && defined(TARGET_LIKE_STM32)
                blink(K_LED_ORANGE);
                #endif
                break;
            default:
                csp_service_handler(conn, packet);
                break;
            }
        }
        csp_close(conn);
    }
    return CSP_TASK_RETURN;
}


CSP_DEFINE_TASK(task_client) {

    csp_packet_t * packet;
    csp_conn_t * conn;

    /* Create a telemetry packet with some dummy test data */
    telemetry_packet data;
    data.data.i = 111;
    data.timestamp = 222;
    data.source.data_type = TELEMETRY_TYPE_INT;
    data.source.source_id = 3;

    while (1) {

        csp_sleep_ms(1000);

        int result = csp_ping(TELEMETRY_CSP_ADDRESS, 100, 100, CSP_O_NONE);
        printf("Ping result %d [ms]\r\n", result);

        csp_sleep_ms(1000);

        packet = csp_buffer_get(100);
        if (packet == NULL) {
            printf("Failed to get buffer element\n");
            return CSP_TASK_RETURN;
        }

        conn = csp_connect(CSP_PRIO_NORM, TELEMETRY_CSP_ADDRESS, TELEMETRY_BEACON_PORT, 1000, CSP_O_NONE);
        if (conn == NULL) {
            printf("Connection failed\n");
            csp_buffer_free(packet);
            return CSP_TASK_RETURN;
        }

        memcpy(packet->data, &data, sizeof(telemetry_packet));
        packet->length = sizeof(telemetry_packet);

        if (!csp_send(conn, packet, 1000)) {
            printf("Send failed\n");
            csp_buffer_free(packet);
        }
        csp_close(conn);
    }
    return CSP_TASK_RETURN;
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

    /* Server */
    printf("Starting Server task\r\n");
    csp_thread_handle_t handle_server;
    csp_thread_create(task_server, "SERVER", 1000, NULL, 0, &handle_server);

     /*Client */
    /*printf("Starting Client task\r\n");*/
    /*csp_thread_handle_t handle_client;*/
    /*csp_thread_create(task_client, "CLIENT", 1000, NULL, 0, &handle_client);*/

    vTaskStartScheduler();
    /* Wait for execution to end (ctrl+c) */
    while(1) {
    }

    return 0;
}
