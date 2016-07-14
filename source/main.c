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

#include "kubos-core/modules/fatfs/stm32f4/diskio.h"

#include <csp/csp.h>

static inline void blink(int pin) {
    k_gpio_write(pin, 1);
    vTaskDelay(5);
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

void task_logging(void *p)
{
    char buffer[128];
    volatile uint16_t num = 0;
    while (1)
    {
        blink(K_LED_ORANGE);
        while ((num = k_uart_read(K_UART_CONSOLE, buffer, 128)) > 0)
        {
            write_string(buffer, num);
            blink(K_LED_BLUE);
        }
        vTaskDelay(1);
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

    xTaskCreate(task_logging, "logging", configMINIMAL_STACK_SIZE * 4, NULL, 2, NULL);

    vTaskStartScheduler();

    return 0;
}
