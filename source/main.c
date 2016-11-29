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
#include <telemetry/telemetry/telemetry.h>
#include <csp/csp.h>
#include <csp/arch/csp_thread.h>
#include <csp/arch/csp_time.h>
#include "telemetry_storage.h"

#ifdef TARGET_LIKE_FREERTOS
#include "kubos-hal/uart.h"
#endif


CSP_DEFINE_TASK(telemetry_log_thread)
{
    int running = 1;
    csp_socket_t *socket = csp_socket(CSP_SO_NONE);
    csp_conn_t *conn;
    csp_packet_t *packet;

    csp_bind(socket, CSP_ANY);
    csp_listen(socket, 5);

    while(running) {
        if( (conn = csp_accept(socket, 10000)) == NULL ) {
            continue;
        }

        while( (packet = csp_read(conn, 100)) != NULL ) {
            switch( csp_conn_dport(conn) ) {
                case CSP_ANY:
                    telemetry_store(packet);
                    csp_buffer_free(packet);
                    break;
                default:
                    csp_service_handler(conn, packet);
                    break;
            }
        }

        csp_close(conn);
    }
}


int main(void)
{
    #ifdef TARGET_LIKE_FREERTOS
    k_uart_console_init();
    #endif

    #ifdef TARGET_LIKE_MSP430
    /* Stop the watchdog. */
    WDTCTL = WDTPW + WDTHOLD;

    __enable_interrupt();
    #endif

    printf("Initialising CSP\r\n");
    csp_buffer_init(5, 20);

    /* Init CSP with address MY_ADDRESS */
    csp_init(1);

    /* Start router task with 500 word stack, OS task priority 1 */
    csp_route_start_task(50, 1);

    csp_thread_handle_t telemetry_log_handle; 
    csp_thread_create(telemetry_log_thread, "TELEMETRY_RX", 50, NULL, 0, &telemetry_log_handle);

    #ifdef TARGET_LIKE_FREERTOS
    vTaskStartScheduler();
    #endif

    /* Wait for program to terminate (ctrl + c) */
    while(1) {
        csp_sleep_ms(1000000);
    }

    return 0;
}
