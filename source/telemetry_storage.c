#include <stdlib.h>
#include <stdio.h>
#include <telemetry/telemetry/telemetry.h>
#include "disk.h"
#include "telemetry_storage.h"

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
 */
static void create_filename(char *filename_buf_ptr, uint8_t source_id, unsigned int address, const char *file_extension)
{
    int len;

    if (filename_buf_ptr == NULL || file_extension == NULL) {
        return;
    }

    len = snprintf(filename_buf_ptr, FILE_NAME_BUFFER_SIZE, "%u%u%s", source_id, address, file_extension);

    if(len < 0 || len >= FILE_NAME_BUFFER_SIZE) {
        printf("Filename char limit exceeded. Have %d, need %d + \\0\n", FILE_NAME_BUFFER_SIZE, len);
        len = snprintf(filename_buf_ptr, FILE_NAME_BUFFER_SIZE, "\0");
    }
}


/**
 * @brief creates a formatted log entry from the telemetry packet.
 * @param data_buf_ptr a pointer to the char[] to write to.
 * @param packet a telemetry packet to create a log entry from.
 * @retval The length of the log entry.
 */
static int format_log_entry_csv(char *data_buf_ptr, telemetry_packet packet) {
    
    int len;

    if (data_buf_ptr == NULL) {
        return 0;
    }

    if(packet.source.data_type == TELEMETRY_TYPE_INT) {
        len = snprintf(data_buf_ptr, DATA_BUFFER_SIZE, "%hu,%d\r\n", packet.timestamp, packet.data.i);
        if(len < 0 || len >= DATA_BUFFER_SIZE) {
            printf("Data char limit exceeded for int packet. Have %d, need %d + \\0\n", DATA_BUFFER_SIZE, len);
            //len = snprintf(data_buf_ptr, FILE_NAME_BUFFER_SIZE, "\0");
            return 0;
        }
    return len;
    }

    if(packet.source.data_type == TELEMETRY_TYPE_FLOAT) {
        len = snprintf(data_buf_ptr, DATA_BUFFER_SIZE, "%hu,%f\r\n", packet.timestamp, packet.data.f);
        if(len < 0 || len >= DATA_BUFFER_SIZE) {
            printf("Data char limit exceeded for float packet. Have %d, need %d + \\0\n", DATA_BUFFER_SIZE, len);
            //len = snprintf(data_buf_ptr, FILE_NAME_BUFFER_SIZE, "\0");
            return 0;
        }
    return len;
    }
}


/**
 * @brief For now filter out any negative return values of snprintf.
 * TODO: handle negative error return values.
 */
static int length_added( int result_of_snprintf )
{
    return (result_of_snprintf > 0) ? result_of_snprintf : 0;
}


/**
 * @brief [WIP] creates a raw hex dump of the entire CSP packet.
 * @param data_buf_ptr a pointer to the char[] to write to.
 * @param packet a pointer to the CSP packet.
 * @param n the size of the csp packet struct.
 */
static void format_log_entry_hex(char *data_buf_ptr, csp_packet_t *packet, size_t n) {
    
    if (data_buf_ptr == NULL || packet == NULL) {
        return;
    }
    int len = 0;
    size_t i = 0;
    
    while(i<n){
        len += length_added(snprintf(data_buf_ptr+len,DATA_BUFFER_SIZE-len, "%02X", packet[i]));
        i++;
    }
    len += length_added(snprintf(data_buf_ptr+len,DATA_BUFFER_SIZE-len, "\r\n", packet[i]));
}


void telemetry_store(csp_packet_t *packet)
{
    static char filename_buffer[FILE_NAME_BUFFER_SIZE];
    static char *filename_buf_ptr;
    static char data_buffer[DATA_BUFFER_SIZE];
    static char *data_buf_ptr;
    static unsigned int csp_address;
    telemetry_packet data;
    uint16_t data_len;
    
    filename_buf_ptr = filename_buffer;
    data_buf_ptr = data_buffer;

    csp_address = packet->id.src;
    data = *(telemetry_packet*)packet->data;
    
    create_filename(filename_buf_ptr, data.source.source_id, csp_address, FILE_EXTENSION_CSV);
    data_len = format_log_entry_csv(data_buf_ptr, data);
    
    task_logging(filename_buf_ptr, data_buf_ptr, data_len);

    /*log here*/
    printf("Log Entry = %s", data_buf_ptr);
    printf("Filename = %s", filename_buf_ptr);
    printf("The data length %u\r\n", data_len);
}
