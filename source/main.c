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

#include "disk.h"
#include "misc.h"

#include "kubos-hal/gpio.h"
#include "kubos-hal/uart.h"

#include <csp/csp.h>

#define FILE_PATH "data.txt"

void task_logging(void *p)
{
    static FATFS FatFs;
    static FIL Fil;
    char buffer[128];
    uint16_t num = 0;
    uint16_t sd_stat = FR_OK;
    uint16_t sync_count = 0;

    sd_stat = open_file(&FatFs, &Fil, FILE_PATH);
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
                sd_stat = open_file(&FatFs, &Fil, FILE_PATH);
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

int main(void)
{
    k_uart_console_init();

    k_gpio_init(K_LED_GREEN, K_GPIO_OUTPUT, K_GPIO_PULL_NONE);
    k_gpio_init(K_LED_ORANGE, K_GPIO_OUTPUT, K_GPIO_PULL_NONE);
    k_gpio_init(K_LED_RED, K_GPIO_OUTPUT, K_GPIO_PULL_NONE);
    k_gpio_init(K_LED_BLUE, K_GPIO_OUTPUT, K_GPIO_PULL_NONE);
    k_gpio_init(K_BUTTON_0, K_GPIO_INPUT, K_GPIO_PULL_NONE);

    xTaskCreate(task_logging, "logging", configMINIMAL_STACK_SIZE * 5, NULL, 2, NULL);

    vTaskStartScheduler();

    return 0;
}
