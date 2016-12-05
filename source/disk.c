#include "disk.h"
#include "misc.h"
#include "kubos-hal/gpio.h"
//#include "kubos-hal/uart.h"
#include <kubos-core/modules/fatfs/ff.h>
#include <kubos-core/modules/fatfs/diskio.h>
#include <kubos-core/modules/fs/fs.h>

static uint16_t open_append(FIL * fp, const char * path)
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

static uint16_t write_string(char * str, int len, const char * path)
{
    static FATFS FatFs;
    static FIL Fil;
    uint16_t ret;
    //uint16_t bw;
    UINT bw;
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

static uint16_t open_file(FATFS * FatFs, FIL * Fil, const char * path)
{
    uint16_t ret;
    if ((ret = f_mount(FatFs, "", 1)) == FR_OK)
    {
        ret = open_append(Fil, path);
    }
    //f_mount(NULL, "", 0);
    return ret;
}


static uint16_t just_write(FIL * Fil, char * str, uint16_t len)
{
    uint16_t ret;
    //uint16_t bw;
    UINT bw;
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


void task_logging(const char *file_path, char *data_buffer, uint16_t data_len)
{
    static FATFS FatFs;
    static FIL Fil;
    uint16_t sd_stat = FR_OK;
    printf("Got to task logging\r\n");

    sd_stat = open_file(&FatFs, &Fil, file_path);

    if (sd_stat != FR_OK)
    {
        blink(K_LED_RED);
        f_close(&Fil);
        f_mount(NULL, "", 0);
        sd_stat = open_file(&FatFs, &Fil, file_path);
    }
    else
    {
        blink(K_LED_BLUE);
        sd_stat = just_write(&Fil, data_buffer, data_len);
        
        if (sd_stat == FR_OK)
        {
            f_sync(&Fil);
            f_close(&Fil);
        }
    }
}


