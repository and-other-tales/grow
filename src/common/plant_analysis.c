#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include <stdio.h>

#include "plant_analysis.h"
#include "ml_analysis.h"
#include "habitat_data.h"
#include "../connectivity.h"

LOG_MODULE_REGISTER(plant_analysis, CONFIG_LOG_DEFAULT_LEVEL);

/* Static sensor data buffer */
static struct sensor_data_with_history sensor_data;
static struct habitat_data habitat_data;

/**
 * @brief Initialize plant analysis subsystem
 * 
 * @return 0 on success, negative errno on failure
 */
int plant_analysis_init(void)
{
    int ret;
    
    /* Initialize ML analysis */
    ret = ml_analysis_init();
    if (ret < 0) {
        LOG_ERR("Failed to initialize ML analysis: %d", ret);
        return ret;
    }
    
    /* Initialize habitat data module */
    ret = habitat_data_init();
    if (ret < 0) {
        LOG_ERR("Failed to initialize habitat data: %d", ret);
        return ret;
    }
    
    /* Clear sensor data */
    memset(&sensor_data, 0, sizeof(sensor_data));
    
    LOG_INF("Plant analysis module initialized");
    return 0;
}

/**
 * @brief Process new sensor readings
 * 
 * This function:
 * 1. Updates sensor history
 * 2. Fetches/loads habitat data if needed
 * 3. Runs ML analysis
 * 4. Returns analysis results
 * 
 * @param serial_number Device serial number
 * @param plant_name Plant name
 * @param plant_variety Plant variety
 * @param soil_moisture Current soil moisture reading
 * @param light_level Current light level reading
 * @param temperature Current temperature reading
 * @param humidity Current humidity reading
 * @param air_movement Current air movement reading
 * @param result_out Pointer to store analysis results
 * @return 0 on success, negative errno on failure
 */
int plant_analysis_process_reading(const char *serial_number,
                                 const char *plant_name,
                                 const char *plant_variety,
                                 float soil_moisture,
                                 float light_level,
                                 float temperature,
                                 float humidity,
                                 float air_movement,
                                 struct ml_analysis_result *result_out)
{
    int ret;
    
    /* Load sensor history if not already loaded */
    static bool history_loaded = false;
    if (!history_loaded) {
        ret = ml_load_sensor_history(serial_number, &sensor_data);
        if (ret < 0 && ret != -ENOENT) {
            LOG_WRN("Failed to load sensor history: %d", ret);
            /* Continue anyway */
        }
        history_loaded = true;
    }
    
    /* Add new sensor reading to history */
    ret = ml_add_sensor_reading(&sensor_data, soil_moisture, light_level,
                               temperature, humidity, air_movement);
    if (ret < 0) {
        LOG_ERR("Failed to add sensor reading: %d", ret);
        return ret;
    }
    
    /* Save updated history */
    ret = ml_save_sensor_history(serial_number, &sensor_data);
    if (ret < 0) {
        LOG_WRN("Failed to save sensor history: %d", ret);
        /* Continue anyway */
    }
    
    /* Try to fetch habitat data if connected, otherwise use cached data */
    ret = habitat_data_fetch(plant_name, plant_variety, &habitat_data);
    if (ret < 0) {
        LOG_WRN("Failed to fetch habitat data: %d", ret);
        
        /* Try to load from cache instead */
        ret = habitat_data_load_cache(plant_name, plant_variety, &habitat_data);
        if (ret < 0) {
            LOG_ERR("Failed to load habitat data from cache: %d", ret);
            
            /* Set default habitat values */
            habitat_data.ideal_temperature_min = 18.0;
            habitat_data.ideal_temperature_max = 26.0;
            habitat_data.ideal_humidity_min = 40.0;
            habitat_data.ideal_humidity_max = 70.0;
            habitat_data.ideal_soil_moisture_min = 30.0;
            habitat_data.ideal_soil_moisture_max = 70.0;
            habitat_data.ideal_light_level_min = 30.0;
            habitat_data.ideal_light_level_max = 80.0;
            habitat_data.data_valid = true;
        }
    }
    
    /* Perform ML analysis */
    ret = ml_analyze_plant_health(&sensor_data, &habitat_data, result_out);
    if (ret < 0) {
        LOG_ERR("Failed to analyze plant health: %d", ret);
        return ret;
    }
    
    LOG_INF("Plant analysis completed - Health: %d, Confidence: %.2f",
           result_out->health_status, result_out->confidence);
    
    return 0;
}

/**
 * @brief Get environmental mismatch string
 * 
 * @param result Analysis result
 * @param output_str Output string buffer
 * @param output_size Size of output buffer
 * @return 0 on success, negative errno on failure
 */
int plant_analysis_get_mismatch_string(const struct ml_analysis_result *result,
                                     char *output_str,
                                     size_t output_size)
{
    if (!result || !output_str || output_size == 0) {
        return -EINVAL;
    }
    
    /* Build mismatch string */
    int written = 0;
    
    if (result->environmental_mismatch.temperature) {
        written += snprintf(output_str + written, output_size - written, "temp,");
    }
    
    if (result->environmental_mismatch.humidity) {
        written += snprintf(output_str + written, output_size - written, "humid,");
    }
    
    if (result->environmental_mismatch.soil_moisture) {
        written += snprintf(output_str + written, output_size - written, "moist,");
    }
    
    if (result->environmental_mismatch.light_level) {
        written += snprintf(output_str + written, output_size - written, "light,");
    }
    
    /* Remove trailing comma if any */
    if (written > 0 && output_str[written - 1] == ',') {
        output_str[written - 1] = '\0';
    } else if (written == 0) {
        /* No mismatches */
        strncpy(output_str, "none", output_size - 1);
        output_str[output_size - 1] = '\0';
    }
    
    return 0;
}

/**
 * @brief Get plant status string based on analysis results
 * 
 * @param result Analysis result
 * @param output_str Output string buffer
 * @param output_size Size of output buffer
 * @return 0 on success, negative errno on failure
 */
int plant_analysis_get_status_string(const struct ml_analysis_result *result, 
                                  char *output_str, 
                                  size_t output_size) 
{
    if (!result || !output_str || output_size == 0) {
        return -EINVAL;
    }
    
    if (result->health_status == ML_HEALTH_CRITICAL) {
        strncpy(output_str, "Critical", output_size - 1);
    } else if (result->health_status == ML_HEALTH_STRESSED) {
        strncpy(output_str, "Stressed", output_size - 1);
    } else {
        /* Check for any mismatch */
        if (result->environmental_mismatch.temperature || 
            result->environmental_mismatch.humidity ||
            result->environmental_mismatch.soil_moisture ||
            result->environmental_mismatch.light_level) {
            strncpy(output_str, "Adjustment Needed", output_size - 1);
        } else {
            strncpy(output_str, "Healthy", output_size - 1);
        }
    }
    
    output_str[output_size - 1] = '\0';
    return 0;
}