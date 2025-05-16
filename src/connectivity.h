#ifndef CONNECTIVITY_H
#define CONNECTIVITY_H

#include <stdbool.h>

/**
 * @brief Initialize connectivity subsystem
 *
 * @return 0 on success, negative errno on failure
 */
int connectivity_init(void);

/**
 * @brief Connect to network
 *
 * @return 0 on success, negative errno on failure
 */
int connectivity_connect(void);

/**
 * @brief Disconnect from network
 *
 * @return 0 on success, negative errno on failure
 */
int connectivity_disconnect(void);

/**
 * @brief Check if connected to network
 *
 * @return true if connected, false otherwise
 */
bool connectivity_is_connected(void);

/**
 * @brief Callback for connectivity status changes
 */
void connectivity_status_callback(bool connected);

#endif /* CONNECTIVITY_H */