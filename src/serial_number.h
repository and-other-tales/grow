#ifndef SERIAL_NUMBER_H
#define SERIAL_NUMBER_H

/**
 * @brief Initialize serial number
 *
 * Generates or loads a unique serial number based on ESP32 MAC address
 *
 * @param serial_out Buffer to store the serial number
 * @param serial_len Length of the output buffer
 *
 * @return 0 on success, negative errno on failure
 */
int serial_number_init(char *serial_out, size_t serial_len);

#endif /* SERIAL_NUMBER_H */