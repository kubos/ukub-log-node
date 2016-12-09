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

#include "telemetry_storage.h"
#include <telemetry/telemetry/telemetry.h>

#ifdef TARGET_LIKE_FREERTOS
#include "kubos-hal/uart.h"
#include "kubos-hal/gpio.h"
#include "misc.h"
#endif

/* Example defines */
#define MY_ADDRESS  1           // Address of local CSP node
#define MY_PORT     10          // Port to send test traffic to

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
            case MY_PORT:
                data = *((telemetry_packet*)packet->data);
                telemetry_store(data, MY_ADDRESS);
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
        
        int result = csp_ping(MY_ADDRESS, 100, 100, CSP_O_NONE);
        printf("Ping result %d [ms]\r\n", result);

        csp_sleep_ms(1000);

        packet = csp_buffer_get(100);
        if (packet == NULL) {
            printf("Failed to get buffer element\n");
            return CSP_TASK_RETURN;
        }

        conn = csp_connect(CSP_PRIO_NORM, MY_ADDRESS, MY_PORT, 1000, CSP_O_NONE);
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


int main(void)
{
    #ifdef TARGET_LIKE_FREERTOS
    k_uart_console_init();
    #endif
    
    #ifdef TARGET_LIKE_STM32
    k_gpio_init(K_LED_GREEN, K_GPIO_OUTPUT, K_GPIO_PULL_NONE);
    k_gpio_init(K_LED_ORANGE, K_GPIO_OUTPUT, K_GPIO_PULL_NONE);
    k_gpio_init(K_LED_RED, K_GPIO_OUTPUT, K_GPIO_PULL_NONE);
    k_gpio_init(K_LED_BLUE, K_GPIO_OUTPUT, K_GPIO_PULL_NONE);
    #endif
    
    #ifdef TARGET_LIKE_MSP430
    k_gpio_init(K_LED_GREEN, K_GPIO_OUTPUT, K_GPIO_PULL_NONE);
    k_gpio_init(K_LED_RED, K_GPIO_OUTPUT, K_GPIO_PULL_NONE);
    /* Stop the watchdog. */
    WDTCTL = WDTPW + WDTHOLD;

    __enable_interrupt();
    #endif
    
    /* Init buffer system with 5 packets of maximum 300 bytes each */
    printf("Initialising CSP\r\n");
    csp_buffer_init(5, 300);

    /* Init CSP with address MY_ADDRESS */
    csp_init(MY_ADDRESS);

    /* Start router task with 300 word stack, OS task priority 1 */
    csp_route_start_task(300, 1);

    /* Server */
    printf("Starting Server task\r\n");
    csp_thread_handle_t handle_server;
    csp_thread_create(task_server, "SERVER", 1000, NULL, 0, &handle_server);

    /* Client */
    printf("Starting Client task\r\n");
    csp_thread_handle_t handle_client;
    csp_thread_create(task_client, "CLIENT", 1000, NULL, 0, &handle_client);
    
    #ifdef TARGET_LIKE_FREERTOS
    vTaskStartScheduler();
    #endif
    
    /* Wait for execution to end (ctrl+c) */
    while(1) {
        csp_sleep_ms(100000);
    }

    return 0;
}
