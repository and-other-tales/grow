#ifndef DATA_CACHE_H
#define DATA_CACHE_H

#include <stdint.h>
#include <stdbool.h>

/* Maximum number of cached entries */
#define MAX_CACHED_ENTRIES 48  // 48 hours of data

/* Structure for a cached sensor reading */
struct cached_sensor_reading {
    float soil_moisture;
    float light_level;
    float temperature;
    float humidity;
    float air_movement;
    int64_t timestamp;
    int health_status;
    char env_mismatch[32];
    char plant_status[32];
    bool valid;
};

/**
 * @brief Initialize data cache
 * 
 * @return 0 on success, negative errno on failure
 */
int data_cache_init(void);

/**
 * @brief Add sensor reading to cache
 * 
 * @param soil_moisture Soil moisture value
 * @param light_level Light level value
 * @param temperature Temperature value
 * @param humidity Humidity value
 * @param air_movement Air movement value
 * @param timestamp Timestamp of reading
 * @param health_status Health status value
 * @param env_mismatch Environmental mismatch flags
 * @param plant_status Plant status string
 * @return 0 on success, negative errno on failure
 */
int data_cache_add_reading(float soil_moisture,
                         float light_level,
                         float temperature,
                         float humidity,
                         float air_movement,
                         int64_t timestamp,
                         int health_status,
                         const char *env_mismatch,
                         const char *plant_status);

/**
 * @brief Get number of cached readings
 * 
 * @return Number of valid cached entries
 */
int data_cache_count(void);

/**
 * @brief Get a cached reading
 * 
 * @param index Index of the reading to get
 * @param reading_out Pointer to store the reading
 * @return 0 on success, negative errno on failure
 */
int data_cache_get_reading(int index, struct cached_sensor_reading *reading_out);

/**
 * @brief Clear cache after successful upload
 * 
 * @return 0 on success, negative errno on failure
 */
int data_cache_clear(void);

/**
 * @brief Save cache to storage
 * 
 * @param serial_number Device serial number
 * @return 0 on success, negative errno on failure
 */
int data_cache_save(const char *serial_number);

/**
 * @brief Load cache from storage
 * 
 * @param serial_number Device serial number
 * @return 0 on success, negative errno on failure
 */
int data_cache_load(const char *serial_number);

#endif /* DATA_CACHE_H */