#ifndef HABITAT_DATA_H
#define HABITAT_DATA_H

#include <stdbool.h>
#include <stddef.h>

#define HABITAT_API_URL "https://grow.othertales.co/api/habitat"

/**
 * @brief Structure containing plant habitat data
 */
struct habitat_data {
    char plant_id[64];
    float ideal_temperature_min;
    float ideal_temperature_max;
    float ideal_humidity_min;
    float ideal_humidity_max;
    float ideal_soil_moisture_min;
    float ideal_soil_moisture_max;
    float ideal_light_level_min;
    float ideal_light_level_max;
    char native_region[64];
    char growing_season[64];
    bool data_valid;
    int64_t timestamp;
};

/**
 * @brief Initialize habitat data module
 * 
 * @return 0 on success, negative errno on failure
 */
int habitat_data_init(void);

/**
 * @brief Fetch habitat data for a plant
 * 
 * @param plant_name Name of the plant
 * @param plant_variety Variety of the plant
 * @param data_out Pointer to store habitat data
 * @return 0 on success, negative errno on failure
 */
int habitat_data_fetch(const char *plant_name, const char *plant_variety, 
                      struct habitat_data *data_out);

/**
 * @brief Cache habitat data to storage
 * 
 * @param data Habitat data to cache
 * @return 0 on success, negative errno on failure
 */
int habitat_data_cache(const struct habitat_data *data);

/**
 * @brief Load habitat data from cache
 * 
 * @param plant_name Name of the plant
 * @param plant_variety Variety of the plant
 * @param data_out Pointer to store habitat data
 * @return 0 on success, negative errno on failure
 */
int habitat_data_load_cache(const char *plant_name, const char *plant_variety,
                           struct habitat_data *data_out);

#endif /* HABITAT_DATA_H */