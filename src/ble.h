#ifndef BLE_H
#define BLE_H

#include <stdbool.h>

/**
 * @brief Initialize BLE subsystem
 *
 * @param provisioned Pointer to device provisioning state
 * @return 0 on success, negative errno on failure
 */
int ble_init(bool *provisioned);

/**
 * @brief Restart BLE advertising for provisioning
 *
 * Called when device needs to be re-provisioned
 * 
 * @return 0 on success, negative errno on failure
 */
int ble_restart_advertising(void);

#endif /* BLE_H */