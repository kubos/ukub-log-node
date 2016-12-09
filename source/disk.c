/*
 * KubOS Core Flight Services
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
#include <ctype.h>
#include "disk.h"
#include "misc.h"
#include "kubos-hal/gpio.h"

#include <kubos-core/modules/fatfs/ff.h>
#include <kubos-core/modules/fatfs/diskio.h>
#include <kubos-core/modules/fs/fs.h>


static uint16_t open_append(FIL *fp, const char *path)
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


static uint16_t write_string(char *str, int len, const char *path)
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


static uint16_t open_file_write(FATFS *FatFs, FIL *Fil, const char *path)
{
    uint16_t ret;

    if ((ret = f_mount(FatFs, "", 1)) == FR_OK)
    {
        ret = open_append(Fil, path);
    }
    //f_mount(NULL, "", 0);
    return ret;
}


static uint16_t open_file_read(FATFS *FatFs, FIL *Fil, const char *path)
{
    uint16_t ret;
    
    if ((ret = f_mount(FatFs, "", 1)) == FR_OK)
    {
        ret = f_open(Fil, path, FA_READ | FA_OPEN_EXISTING);
        printf("** Opened file: %d\r\n", ret);
    }

    return ret;
}


/** 
 * The close_file function closes a file and releases the handle.
 * @param Fil a pointer to the file object structure
 * @return ret a table of values which (0 being 'okay') is found at
 * http://elm-chan.org/fsw/ff/en/rc.html  
 */
static uint16_t close_file(FIL *Fil)
{
    uint16_t ret;
    ret = f_close(Fil);

    return ret;
}


static uint16_t just_write(FIL *Fil, char *str, uint16_t len)
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


/**
 * The read_value function checks for an available string to read
 * from the calibration profile file and, if it is available, reads the 
 * string.
 * 
 * @param Fil,  pointer to the file object structure
 * @return ret, 0 being 'FR_OK') 
 * and 20 being either 'at the end of tile' or 21 'this thing is not a digit'.
 */
uint16_t read_value(FIL *Fil, uint16_t *value)
{
    uint16_t ret = FR_OK;
    uint16_t temp = 0;
    int c;
    char buffer[128];

    /* Make sure there's something to read */
    if(f_eof(Fil))
    {
        return 20;
    }

    f_gets(buffer, sizeof buffer, Fil);

    if(!isdigit(buffer[0]))
    {
        return 20;
    }

    /* convert read string to uint */
    for (c = 0; isdigit(buffer[c]); c++)
    {
        temp = temp * 10 + buffer[c] - '0';
    }

    *value = temp;

    return ret;
}


/** 
 * write_value: a function to write a calibration profile value to the file.
 * If successful, the green LED blinks.
 * @param Fil, pointer to the file object structure
 * @param value, the thing you want to write to the file
 * @return ret returns 0 for success and 21 (for failure.  
 */
uint16_t write_value(FIL *Fil, uint16_t value)
{
    if ((f_printf(Fil, "%d\n", value)) != -1)
    {
        blink(K_LED_GREEN);
        return  0;
    }
    blink(K_LED_RED);
    return 22;
}


uint16_t disk_save_string(const char *file_path, char *data_buffer, uint16_t data_len)
{
    static FATFS FatFs;
    static FIL Fil;
    uint16_t sd_stat = FR_OK;

    sd_stat = open_file_write(&FatFs, &Fil, file_path);
    
    if (sd_stat != FR_OK)
    {
        blink(K_LED_RED);
        close_file(&Fil);
        f_mount(NULL, "", 0);
        sd_stat = open_file_write(&FatFs, &Fil, file_path);
    }

    if (sd_stat == FR_OK)
    {
        blink(K_LED_GREEN);
        sd_stat = just_write(&Fil, data_buffer, data_len);
        close_file(&Fil);
        return sd_stat;
    }

    return sd_stat;
}


uint16_t disk_load_uint16(const char *file_path, uint16_t *value)
{
    static FATFS FatFs;
    static FIL Fil;
    uint16_t sd_stat = FR_OK;
    
    sd_stat = open_file_read(&FatFs, &Fil, file_path);

    if (sd_stat == FR_OK)
    {
        blink(K_LED_GREEN);
        sd_stat = read_value(&Fil, value);
        close_file(&Fil);
        return sd_stat;
    }

    return sd_stat;
}


uint16_t disk_save_uint16(const char *file_path, uint16_t value)
{
    static FATFS FatFs;
    static FIL Fil;
    uint16_t sd_stat = FR_OK;
    
    sd_stat = open_file_write(&FatFs, &Fil, file_path);

    if (sd_stat != FR_OK)
    {
        blink(K_LED_RED);
        close_file(&Fil);
        f_mount(NULL, "", 0);
        sd_stat = open_file_write(&FatFs, &Fil, file_path);
    }
    
    if (sd_stat == FR_OK)
    {
        blink(K_LED_GREEN);
        sd_stat = write_value(&Fil, value);
        close_file(&Fil);
        return sd_stat;
    }

    return sd_stat;
}

