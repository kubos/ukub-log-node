#include <stdlib.h>
#include <stdio.h>
#include <csp/csp.h>
#include <telemetry/telemetry.h>
#include "disk.h"
#include "telemetry_storage.h"
#include <kubos-hal/gpio.h>

/* Set at FatFs LFN max length */
#define FILE_NAME_BUFFER_SIZE 255
#define DATA_BUFFER_SIZE 128

#define FILE_EXTENSION_HEX ".hex"
#define FILE_EXTENSION_CSV ".csv"


/**
 * @brief creates a filename that corresponds to the telemetry packet source_id and 
 *        the csp packet address.
 * @param filename_buf_ptr a pointer to the char[] to write to.
 * @param source_id the telemetry packet source_id from packet.source.source_id.
 * @param address the csp packet address from packet->id.src. 
 * @param file_extension. 
 * @retval The length of the filename written.
 */
static int create_filename(char *filename_buf_ptr, uint8_t source_id, unsigned int address, const char *file_extension)
{
    int len;

    if (filename_buf_ptr == NULL || file_extension == NULL) {
        return 0;
    }

    len = snprintf(filename_buf_ptr, FILE_NAME_BUFFER_SIZE, "%u%u%s", source_id, address, file_extension);

    if(len < 0 || len >= FILE_NAME_BUFFER_SIZE) {
        printf("Filename char limit exceeded. Have %d, need %d + \\0\n", FILE_NAME_BUFFER_SIZE, len);
        //len = snprintf(filename_buf_ptr, FILE_NAME_BUFFER_SIZE, "\0");
        return 0;
    }
    return len;
}


/**
 * @brief creates a formatted log entry from the telemetry packet.
 * @param data_buf_ptr a pointer to the char[] to write to.
 * @param packet a telemetry packet to create a log entry from.
 * @retval The length of the log entry written.
 */
static int format_log_entry_csv(char *data_buf_ptr, telemetry_packet packet) {

    int len;

    if (data_buf_ptr == NULL) {
        return 0;
    }

    if(packet.source.data_type == TELEMETRY_TYPE_INT) {
        len = snprintf(data_buf_ptr, DATA_BUFFER_SIZE, "%u,%d\r\n", packet.timestamp, packet.data.i);
        if(len < 0 || len >= DATA_BUFFER_SIZE) {
            printf("Data char limit exceeded for int packet. Have %d, need %d + \\0\n", DATA_BUFFER_SIZE, len);
            return 0;
        }
    return len; }

    if(packet.source.data_type == TELEMETRY_TYPE_FLOAT) {
        len = snprintf(data_buf_ptr, DATA_BUFFER_SIZE, "%u,%f\r\n", packet.timestamp, packet.data.f);
        if(len < 0 || len >= DATA_BUFFER_SIZE) {
            printf("Data char limit exceeded for float packet. Have %d, need %d + \\0\n", DATA_BUFFER_SIZE, len);
            return 0;
        }
    return len;
    }
}


/**
 * @brief [WIP Stub] creates a raw hex dump of the entire CSP packet.
 * @param data_buf_ptr a pointer to the char[] to write to.
 * @param packet a pointer to the CSP packet.
 */
static void format_log_entry_hex(char *data_buf_ptr, csp_packet_t *packet) {

    if (data_buf_ptr == NULL || packet == NULL) {
        return;
    }
}


/**
 * @brief print telemetry packet data.
 * @param packet a telemetry packet with data to print.
 */
void print_to_console(telemetry_packet packet){

    if(packet.source.data_type == TELEMETRY_TYPE_INT) {
        printf("%d\r\n", packet.data.i);
    }

    if(packet.source.data_type == TELEMETRY_TYPE_FLOAT) {
        printf("%f\r\n", packet.data.f);
    }
}

//TODO: REMOVE ADDRESS
void telemetry_store(telemetry_packet data)
{
    static char filename_buffer[FILE_NAME_BUFFER_SIZE];
    static char *filename_buf_ptr;
    static char data_buffer[DATA_BUFFER_SIZE];
    static char *data_buf_ptr;

    uint16_t data_len;
    uint16_t filename_len;

    /*blink(K_LED_RED);*/
    /*blink(K_LED_RED);*/
    /*[>blink(K_LED_BLUE);<]*/
    /*blink(K_LED_RED);*/
    printf("\n\n\nTelemetry Store\n ");
    filename_buf_ptr = filename_buffer;
    data_buf_ptr = data_buffer;

    filename_len = create_filename(filename_buf_ptr, data.source.source_id, data.source.subsystem_id, FILE_EXTENSION_CSV);
    data_len = format_log_entry_csv(data_buf_ptr, data);

    /*log here*/
    printf("disk save string");
    disk_save_string(filename_buf_ptr, data_buf_ptr, data_len);

    printf("Log Entry = %s", data_buf_ptr);
    printf("Filename = %s", filename_buf_ptr);
    printf("The data length %u\r\n", data_len);
}
