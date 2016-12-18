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
#include "telemetry_storage.h"
#include "disk.h"
#include "telemetry.h"

#ifdef TARGET_LIKE_FREERTOS
#include "kubos-hal/uart.h"
#include "kubos-hal/gpio.h"
#include "misc.h"
#endif


CSP_DEFINE_TASK(telemetry_store_task) {
    
   telemetry_packet packet = { .data.i = 0, .timestamp = 0, \
        .source.subsystem_id = 0, .source.data_type = TELEMETRY_TYPE_INT, \
        .source.source_id = 0};
    
    telemetry_conn connection1;

    /* Subscribe to source id 0x1 */
    while (!telemetry_subscribe(&connection1, 0x1))
    {
        csp_sleep_ms(500);
    }
    
    while (1) {
        
        printf("In task telemetry_log\r\n");
        
        #ifdef TARGET_LIKE_FREERTOS
        blink(K_LED_RED);
        #endif
        
        if (telemetry_read(connection1, &packet)) 
        {
        print_to_console(packet);
        //telemetry_store(packet);
        }
        csp_sleep_ms(1000);
    }
    return CSP_TASK_RETURN;
}


CSP_DEFINE_TASK(dummy_publisher) {
    
    int runcount = 0;
    uint16_t return_val;
    csp_packet_t *packet;
    csp_conn_t *conn;
    
    /* Create a telemetry packet for dummy test data */
    telemetry_packet dummy_packet = { .data.i = 0, .timestamp = 0, \
        .source.subsystem_id = 0x1, .source.data_type = TELEMETRY_TYPE_INT, \
        .source.source_id = 0x1};
    
    while (1) {
        
        printf("In task dummy_publisher\r\n");
        
        #ifdef TARGET_LIKE_FREERTOS
        blink(K_LED_GREEN);
        #endif
        
        /* Alternate between two different dummy payloads */ 
        runcount++;
        if(runcount%2 == 0){
            dummy_packet.data.i = 333;
            dummy_packet.timestamp = 444;
        }
        else {
            dummy_packet.data.i = 111;
            dummy_packet.timestamp = 222;
        }
        
        /* Push the dummy packet into the telemetry system */
        telemetry_publish(dummy_packet);
        csp_sleep_ms(1000);
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

    /* Initialize the telemetry system */
    printf("Initializing the telemetry system \r\n");
    telemetry_init();
    
    /* Dummy publisher task for pushing dummy data into telemetry */
    printf("Starting dummy publisher task\r\n");
    csp_thread_handle_t handle_dummy_publisher;
    csp_thread_create(dummy_publisher, "PUB", 1000, NULL, 0, &handle_dummy_publisher);
    
    /* Logging task for telemetry packets */
    printf("Starting telemetry log task\r\n");
    csp_thread_handle_t handle_telemetry_store_task;
    csp_thread_create(telemetry_store_task, "SUB", 1000, NULL, 0, &handle_telemetry_store_task);

    #ifdef TARGET_LIKE_FREERTOS
    vTaskStartScheduler();
    #endif
    
    /* Wait for execution to end (ctrl+c) */
    while(1) {
        csp_sleep_ms(100000);
    }
    return 0;
}
