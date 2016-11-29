#include "disk.h"
#include "kubos-hal/gpio.h"
#include "misc.h"

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

uint16_t write_string(char * str, int len, const char * path)
{
    static FATFS FatFs;
    static FIL Fil;
    uint16_t ret;
    uint16_t bw;
    if ((ret = f_mount(&FatFs, "", 1)) == FR_OK)
    {
        if ((ret = open_append(&Fil, path)) == FR_OK)
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

uint16_t open_file(FATFS * FatFs, FIL * Fil, const char * path)
{
    uint16_t ret;
    if ((ret = f_mount(FatFs, "", 1)) == FR_OK)
    {
        ret = open_append(Fil, path);
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


void task_logging(void *p, const char *file_path)
{
    static FATFS FatFs;
    static FIL Fil;
    char buffer[128];
    uint16_t num = 0;
    uint16_t sd_stat = FR_OK;
    uint16_t sync_count = 0;

    sd_stat = open_file(&FatFs, &Fil, file_path);
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
                sd_stat = open_file(&FatFs, &Fil, file_path);
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
