#ifndef WIFI_H
#define WIFI_H

#include <stdbool.h>

/**
 * @brief Initialize and connect to WiFi network
 *
 * @return 0 on success, negative errno on failure
 */
int wifi_connect(void);

/**
 * @brief Disconnect from WiFi network
 *
 * @return 0 on success, negative errno on failure
 */
int wifi_disconnect(void);

/**
 * @brief Check if WiFi is connected
 *
 * @return true if connected, false otherwise
 */
bool wifi_is_connected(void);

/* Callback function to be implemented by the user */
void wifi_connected_callback(void);

#endif /* WIFI_H */