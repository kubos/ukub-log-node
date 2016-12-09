#ifndef TELEMETRY_STORAGE_H
#define TELEMETRY_STORAGE_H

#include <telemetry/telemetry/telemetry.h>


/**
 * @brief store a csp packet or packet contents in a particular format.
 * @param packet a pointer to the CSP packet.
 */
void telemetry_store(telemetry_packet data, unsigned int address);

void print_to_console(telemetry_packet data);

#endif
