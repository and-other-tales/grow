#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include <errno.h>

#include "data_cache.h"
#include "storage.h"

LOG_MODULE_REGISTER(data_cache, CONFIG_LOG_DEFAULT_LEVEL);

/* Cache storage */
static struct cached_sensor_reading cache[MAX_CACHED_ENTRIES];
static int cache_head = 0; /* Index for next write */
static int cache_count = 0; /* Number of valid entries */

/**
 * @brief Initialize data cache
 * 
 * @return 0 on success, negative errno on failure
 */
int data_cache_init(void)
{
    /* Clear cache */
    memset(cache, 0, sizeof(cache));
    cache_head = 0;
    cache_count = 0;
    
    LOG_INF("Data cache initialized");
    return 0;
}

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
                         const char *plant_status)
{
    /* Add to circular buffer */
    cache[cache_head].soil_moisture = soil_moisture;
    cache[cache_head].light_level = light_level;
    cache[cache_head].temperature = temperature;
    cache[cache_head].humidity = humidity;
    cache[cache_head].air_movement = air_movement;
    cache[cache_head].timestamp = timestamp;
    cache[cache_head].health_status = health_status;
    
    strncpy(cache[cache_head].env_mismatch, env_mismatch, sizeof(cache[cache_head].env_mismatch) - 1);
    cache[cache_head].env_mismatch[sizeof(cache[cache_head].env_mismatch) - 1] = '\0';
    
    strncpy(cache[cache_head].plant_status, plant_status, sizeof(cache[cache_head].plant_status) - 1);
    cache[cache_head].plant_status[sizeof(cache[cache_head].plant_status) - 1] = '\0';
    
    cache[cache_head].valid = true;
    
    /* Update head index */
    cache_head = (cache_head + 1) % MAX_CACHED_ENTRIES;
    
    /* Update count */
    if (cache_count < MAX_CACHED_ENTRIES) {
        cache_count++;
    }
    
    LOG_DBG("Added reading to cache (total: %d)", cache_count);
    return 0;
}

/**
 * @brief Get number of cached readings
 * 
 * @return Number of valid cached entries
 */
int data_cache_count(void)
{
    return cache_count;
}

/**
 * @brief Get a cached reading
 * 
 * @param index Index of the reading to get
 * @param reading_out Pointer to store the reading
 * @return 0 on success, negative errno on failure
 */
int data_cache_get_reading(int index, struct cached_sensor_reading *reading_out)
{
    if (!reading_out || index < 0 || index >= cache_count) {
        return -EINVAL;
    }
    
    /* Calculate actual index in circular buffer */
    int actual_index = 0;
    if (cache_count < MAX_CACHED_ENTRIES) {
        /* Buffer not full yet */
        actual_index = index;
    } else {
        /* Buffer full, calculate based on head position */
        actual_index = (cache_head + index) % MAX_CACHED_ENTRIES;
    }
    
    if (!cache[actual_index].valid) {
        return -ENOENT;
    }
    
    /* Copy data */
    memcpy(reading_out, &cache[actual_index], sizeof(struct cached_sensor_reading));
    
    return 0;
}

/**
 * @brief Clear cache after successful upload
 * 
 * @return 0 on success, negative errno on failure
 */
int data_cache_clear(void)
{
    memset(cache, 0, sizeof(cache));
    cache_head = 0;
    cache_count = 0;
    
    LOG_INF("Data cache cleared");
    return 0;
}

/**
 * @brief Save cache to storage
 * 
 * @param serial_number Device serial number
 * @return 0 on success, negative errno on failure
 */
int data_cache_save(const char *serial_number)
{
    char key[64];
    
    /* Save cache metadata */
    snprintf(key, sizeof(key), "cache/meta/%s", serial_number);
    struct {
        int head;
        int count;
    } meta = {
        .head = cache_head,
        .count = cache_count
    };
    
    int ret = storage_save_value(key, &meta, sizeof(meta));
    if (ret < 0) {
        LOG_ERR("Failed to save cache metadata: %d", ret);
        return ret;
    }
    
    /* Save cache data */
    snprintf(key, sizeof(key), "cache/data/%s", serial_number);
    ret = storage_save_value(key, cache, sizeof(cache));
    if (ret < 0) {
        LOG_ERR("Failed to save cache data: %d", ret);
        return ret;
    }
    
    LOG_INF("Data cache saved (%d entries)", cache_count);
    return 0;
}

/**
 * @brief Load cache from storage
 * 
 * @param serial_number Device serial number
 * @return 0 on success, negative errno on failure
 */
int data_cache_load(const char *serial_number)
{
    char key[64];
    
    /* Load cache metadata */
    snprintf(key, sizeof(key), "cache/meta/%s", serial_number);
    struct {
        int head;
        int count;
    } meta;
    
    size_t meta_size = sizeof(meta);
    int ret = storage_load_value(key, &meta, &meta_size);
    
    if (ret < 0) {
        if (ret == -ENOENT) {
            /* No cache data, initialize empty cache */
            LOG_INF("No saved cache data found");
            data_cache_init();
            return 0;
        }
        LOG_ERR("Failed to load cache metadata: %d", ret);
        return ret;
    }
    
    if (meta_size != sizeof(meta)) {
        LOG_ERR("Invalid cache metadata size");
        return -EINVAL;
    }
    
    /* Load cache data */
    snprintf(key, sizeof(key), "cache/data/%s", serial_number);
    size_t cache_size = sizeof(cache);
    ret = storage_load_value(key, cache, &cache_size);
    
    if (ret < 0) {
        LOG_ERR("Failed to load cache data: %d", ret);
        return ret;
    }
    
    if (cache_size != sizeof(cache)) {
        LOG_ERR("Invalid cache data size");
        return -EINVAL;
    }
    
    /* Set cache state */
    cache_head = meta.head;
    cache_count = meta.count;
    
    LOG_INF("Data cache loaded (%d entries)", cache_count);
    return 0;
}