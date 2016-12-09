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
#ifndef DISK_H
#define DISK_H

#include <stdio.h>


/**
 * @brief open and appends and writes or if the filename does not exist
 *        creates a new file and writes a string.
 * @param file_path a pointer to the filename to write to.
 * @param data_buffer a pointer to the string to write.
 * @param data_len the length of the string.
 * @param retry_attempts.
 * @retval a table of values which (0 being 'okay') is found at 
 *         http://elm-chan.org/fsw/ff/en/rc.html
 */
uint16_t disk_save_string(const char *file_path, char *data_buffer, uint16_t data_len);


/**
 * @brief open a file and load an unsigned 16 bit integer.
 * @param file_path a pointer to the filename to open.
 * @param value the pointer to store the retrieved value.
 * @retval a table of values which (0 being 'okay') is found at 
 *         http://elm-chan.org/fsw/ff/en/rc.html
 */
uint16_t disk_load_uint16(const char *file_path, uint16_t * value);


/**
 * @brief open and appends and writes or if the filename does not exist
 *        creates a new file and writes an unsigned 16 bit integer.
 * @param file_path a pointer to the filename to open.
 * @param value the value to write.
 * @retval a table of values which (0 being 'okay') is found at 
 *         http://elm-chan.org/fsw/ff/en/rc.html
 */
uint16_t disk_save_uint16(const char *file_path, uint16_t value);


#endif
