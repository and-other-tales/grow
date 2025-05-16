#include <zephyr/kernel.h>
#include <zephyr/settings/settings.h>
#include <zephyr/fs/nvs.h>
#include <zephyr/logging/log.h>
#include <zephyr/storage/flash_map.h>
#include <string.h>

#include "storage.h"

LOG_MODULE_REGISTER(storage, CONFIG_LOG_DEFAULT_LEVEL);

/* Flash partition label for NVS */
#define FLASH_PARTITION storage_partition
#define FLASH_PARTITION_ID FIXED_PARTITION_ID(FLASH_PARTITION)

/* NVS storage defines */
#define NVS_SECTOR_SIZE 4096
#define NVS_SECTOR_COUNT 6
#define NVS_SECTOR_OFFSET FLASH_AREA_OFFSET(FLASH_PARTITION)

/* Settings keys */
#define WIFI_SSID_KEY "wifi/ssid"
#define WIFI_PASSWORD_KEY "wifi/password"
#define PLANT_NAME_KEY "plant/name"
#define PLANT_VARIETY_KEY "plant/variety"
#define PROVISIONED_KEY "device/provisioned"

/* Static NVS instance */
static struct nvs_fs nvs;
static bool storage_initialized = false;

/**
 * @brief Initialize the storage subsystem
 *
 * @return 0 on success, negative errno on failure
 */
int storage_init(void)
{
    int rc;
    const struct flash_area *fa;
    
    if (storage_initialized) {
        return 0;
    }
    
    /* Setup NVS */
    nvs.flash_device = FIXED_PARTITION_DEVICE(FLASH_PARTITION);
    if (nvs.flash_device == NULL) {
        LOG_ERR("Failed to get flash device");
        return -ENODEV;
    }
    
    nvs.offset = NVS_SECTOR_OFFSET;
    nvs.sector_size = NVS_SECTOR_SIZE;
    nvs.sector_count = NVS_SECTOR_COUNT;
    
    rc = nvs_mount(&nvs);
    if (rc < 0) {
        LOG_ERR("Flash init failed: %d", rc);
        return rc;
    }
    
    /* Initialize settings subsystem */
    rc = settings_subsys_init();
    if (rc < 0) {
        LOG_ERR("Settings init failed: %d", rc);
        return rc;
    }
    
    LOG_INF("Storage subsystem initialized");
    storage_initialized = true;
    
    return 0;
}

/**
 * @brief Save a value to NVS storage
 *
 * @param key Key to save
 * @param value Value to save
 * @param value_len Length of value
 * @return 0 on success, negative errno on failure
 */
int storage_save_value(const char *key, const void *value, size_t value_len)
{
    int rc;
    
    if (!storage_initialized) {
        rc = storage_init();
        if (rc < 0) {
            return rc;
        }
    }
    
    uint16_t id = crc16_ccitt(0, key, strlen(key));
    
    rc = nvs_write(&nvs, id, value, value_len);
    if (rc < 0) {
        LOG_ERR("Failed to write to NVS: %d", rc);
        return rc;
    }
    
    return 0;
}

/**
 * @brief Load a value from NVS storage
 *
 * @param key Key to load
 * @param value_out Buffer to store the value
 * @param value_len_inout [in] Size of the buffer, [out] actual size read
 * @return 0 on success, negative errno on failure
 */
int storage_load_value(const char *key, void *value_out, size_t *value_len_inout)
{
    int rc;
    
    if (!storage_initialized) {
        rc = storage_init();
        if (rc < 0) {
            return rc;
        }
    }
    
    uint16_t id = crc16_ccitt(0, key, strlen(key));
    
    rc = nvs_read(&nvs, id, value_out, *value_len_inout);
    if (rc < 0) {
        LOG_ERR("Failed to read from NVS: %d", rc);
        return rc;
    }
    
    *value_len_inout = rc;
    return 0;
}

/**
 * @brief Delete a value from NVS storage
 *
 * @param key Key to delete
 * @return 0 on success, negative errno on failure
 */
int storage_delete_value(const char *key)
{
    int rc;
    
    if (!storage_initialized) {
        rc = storage_init();
        if (rc < 0) {
            return rc;
        }
    }
    
    uint16_t id = crc16_ccitt(0, key, strlen(key));
    
    rc = nvs_delete(&nvs, id);
    if (rc < 0) {
        LOG_ERR("Failed to delete from NVS: %d", rc);
        return rc;
    }
    
    return 0;
}

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
                              const char *plant_name, const char *plant_variety)
{
    int rc;
    bool provisioned = true;
    
    rc = storage_save_value(WIFI_SSID_KEY, wifi_ssid, strlen(wifi_ssid));
    if (rc < 0) {
        return rc;
    }
    
    rc = storage_save_value(WIFI_PASSWORD_KEY, wifi_password, strlen(wifi_password));
    if (rc < 0) {
        return rc;
    }
    
    rc = storage_save_value(PLANT_NAME_KEY, plant_name, strlen(plant_name));
    if (rc < 0) {
        return rc;
    }
    
    rc = storage_save_value(PLANT_VARIETY_KEY, plant_variety, strlen(plant_variety));
    if (rc < 0) {
        return rc;
    }
    
    rc = storage_save_value(PROVISIONED_KEY, &provisioned, sizeof(provisioned));
    if (rc < 0) {
        return rc;
    }
    
    return 0;
}

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
                              bool *provisioned_out)
{
    int rc;
    size_t len;
    bool provisioned = false;
    
    /* Check provisioning first */
    len = sizeof(provisioned);
    rc = storage_load_value(PROVISIONED_KEY, &provisioned, &len);
    if (rc < 0) {
        /* Not provisioned yet */
        *provisioned_out = false;
        return 0;
    }
    
    /* Device is provisioned, load configuration */
    *provisioned_out = provisioned;
    
    if (plant_name_out && plant_name_len > 0) {
        len = plant_name_len - 1;
        rc = storage_load_value(PLANT_NAME_KEY, plant_name_out, &len);
        if (rc < 0) {
            strncpy(plant_name_out, "Unknown", plant_name_len - 1);
        }
        plant_name_out[len] = '\0';
    }
    
    if (plant_variety_out && plant_variety_len > 0) {
        len = plant_variety_len - 1;
        rc = storage_load_value(PLANT_VARIETY_KEY, plant_variety_out, &len);
        if (rc < 0) {
            strncpy(plant_variety_out, "Unknown", plant_variety_len - 1);
        }
        plant_variety_out[len] = '\0';
    }
    
    return 0;
}

/**
 * @brief Reset device configuration (factory reset)
 *
 * @return 0 on success, negative errno on failure
 */
int storage_reset_device_config(void)
{
    int rc;
    
    rc = storage_delete_value(WIFI_SSID_KEY);
    if (rc < 0 && rc != -ENOENT) {
        return rc;
    }
    
    rc = storage_delete_value(WIFI_PASSWORD_KEY);
    if (rc < 0 && rc != -ENOENT) {
        return rc;
    }
    
    rc = storage_delete_value(PLANT_NAME_KEY);
    if (rc < 0 && rc != -ENOENT) {
        return rc;
    }
    
    rc = storage_delete_value(PLANT_VARIETY_KEY);
    if (rc < 0 && rc != -ENOENT) {
        return rc;
    }
    
    rc = storage_delete_value(PROVISIONED_KEY);
    if (rc < 0 && rc != -ENOENT) {
        return rc;
    }
    
    return 0;
}