#ifndef STORAGE_H
#define STORAGE_H

#include <stdbool.h>
#include <stddef.h>

/**
 * @brief Initialize storage subsystem
 *
 * @return 0 on success, negative errno on failure
 */
int storage_init(void);

/**
 * @brief Save a key-value pair to flash
 *
 * @param key Key to save
 * @param value Value to save
 * @param value_len Length of value
 * @return 0 on success, negative errno on failure
 */
int storage_save_value(const char *key, const void *value, size_t value_len);

/**
 * @brief Load a value from flash
 *
 * @param key Key to load
 * @param value_out Buffer to store the value
 * @param value_len_inout [in] Size of the buffer, [out] actual size read
 * @return 0 on success, negative errno on failure
 */
int storage_load_value(const char *key, void *value_out, size_t *value_len_inout);

/**
 * @brief Delete a key-value pair from flash
 *
 * @param key Key to delete
 * @return 0 on success, negative errno on failure
 */
int storage_delete_value(const char *key);

/**
 * @brief Save device configuration to flash
 *
 * @param wifi_ssid WiFi SSID
 * @param wifi_password WiFi password
 * @param plant_name Plant name
 * @param plant_variety Plant variety
 * @return 0 on success, negative errno on failure
 */
int storage_save_device_config(const char *wifi_ssid, const char *wifi_password,
                              const char *plant_name, const char *plant_variety);

/**
 * @brief Load device configuration from flash
 *
 * @param plant_name_out Buffer to store plant name
 * @param plant_name_len Size of plant name buffer
 * @param plant_variety_out Buffer to store plant variety
 * @param plant_variety_len Size of plant variety buffer
 * @param provisioned_out Pointer to store provisioning status
 * @return 0 on success, negative errno on failure
 */
int storage_load_device_config(char *plant_name_out, size_t plant_name_len,
                              char *plant_variety_out, size_t plant_variety_len,
                              bool *provisioned_out);

/**
 * @brief Reset device configuration (factory reset)
 *
 * @return 0 on success, negative errno on failure
 */
int storage_reset_device_config(void);

#endif /* STORAGE_H */