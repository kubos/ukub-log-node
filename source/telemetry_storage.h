#ifndef TELEMETRY_STORAGE_H
#define TELEMETRY_STORAGE_H

#include <telemetry/telemetry.h>


/**
 * @brief store a telemetry packet in a particular format specified by
 *        the configuration.
 * @param packet the telemetry packet to store.
 */
void telemetry_store(telemetry_packet packet, unsigned int address);


/**
 * @brief print telemetry packet data.
 * @param packet a telemetry packet with data to print.
 */
void print_to_console(telemetry_packet data);

#endif
