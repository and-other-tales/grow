#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include "ml_analysis.h"
#include "../tflite_interface.h"
#include "../storage.h"

LOG_MODULE_REGISTER(ml_analysis, CONFIG_LOG_DEFAULT_LEVEL);

/* TensorFlow Lite context */
static struct tflite_context tflite_ctx;

/* History storage */
#define SENSOR_HISTORY_KEY_PREFIX "sensor_history/"
#define SENSOR_HISTORY_KEY_MAX 128

/* Helper functions for environmental mismatch detection */
static bool is_temp_mismatch(float temp, const struct habitat_data *habitat)
{
    return (temp < habitat->ideal_temperature_min) || (temp > habitat->ideal_temperature_max);
}

static bool is_humidity_mismatch(float humidity, const struct habitat_data *habitat)
{
    return (humidity < habitat->ideal_humidity_min) || (humidity > habitat->ideal_humidity_max);
}

static bool is_moisture_mismatch(float moisture, const struct habitat_data *habitat)
{
    return (moisture < habitat->ideal_soil_moisture_min) || (moisture > habitat->ideal_soil_moisture_max);
}

static bool is_light_mismatch(float light, const struct habitat_data *habitat)
{
    return (light < habitat->ideal_light_level_min) || (light > habitat->ideal_light_level_max);
}

/* Helper functions for computing differences from ideal values */
static float compute_temp_diff(float temp, const struct habitat_data *habitat)
{
    float ideal_mid = (habitat->ideal_temperature_min + habitat->ideal_temperature_max) / 2.0f;
    return temp - ideal_mid;
}

static float compute_humidity_diff(float humidity, const struct habitat_data *habitat)
{
    float ideal_mid = (habitat->ideal_humidity_min + habitat->ideal_humidity_max) / 2.0f;
    return humidity - ideal_mid;
}

static float compute_moisture_diff(float moisture, const struct habitat_data *habitat)
{
    float ideal_mid = (habitat->ideal_soil_moisture_min + habitat->ideal_soil_moisture_max) / 2.0f;
    return moisture - ideal_mid;
}

static float compute_light_diff(float light, const struct habitat_data *habitat)
{
    float ideal_mid = (habitat->ideal_light_level_min + habitat->ideal_light_level_max) / 2.0f;
    return light - ideal_mid;
}

/* Generate recommendations based on mismatches */
static void generate_recommendations(struct ml_analysis_result *result)
{
    char *recommendation = result->recommendation;
    size_t rec_size = sizeof(result->recommendation);
    size_t written = 0;
    
    if (result->health_status == ML_HEALTH_HEALTHY) {
        written += snprintf(recommendation + written, rec_size - written,
                          "Plant is healthy. ");
    } else {
        /* Add specific recommendations for each mismatch */
        if (result->environmental_mismatch.temperature) {
            written += snprintf(recommendation + written, rec_size - written,
                              "Adjust temperature. ");
        }
        
        if (result->environmental_mismatch.humidity) {
            written += snprintf(recommendation + written, rec_size - written,
                              "Adjust humidity level. ");
        }
        
        if (result->environmental_mismatch.soil_moisture) {
            written += snprintf(recommendation + written, rec_size - written,
                              "Adjust watering schedule. ");
        }
        
        if (result->environmental_mismatch.light_level) {
            written += snprintf(recommendation + written, rec_size - written,
                              "Adjust light exposure. ");
        }
    }
}

/**
 * @brief Initialize ML analysis module
 * 
 * @return 0 on success, negative errno on failure
 */
int ml_analysis_init(void)
{
    int ret = tflite_init(&tflite_ctx);
    if (ret < 0) {
        LOG_ERR("Failed to initialize TFLite: %d", ret);
        return ret;
    }
    
    LOG_INF("ML analysis module initialized");
    return 0;
}

/**
 * @brief Add sensor reading to history
 * 
 * @param sensor_data Pointer to sensor data structure
 * @param soil_moisture Current soil moisture reading
 * @param light_level Current light level reading
 * @param temperature Current temperature reading
 * @param humidity Current humidity reading
 * @param air_movement Current air movement reading
 * @return 0 on success, negative errno on failure
 */
int ml_add_sensor_reading(struct sensor_data_with_history *sensor_data,
                         float soil_moisture, float light_level,
                         float temperature, float humidity,
                         float air_movement)
{
    if (!sensor_data) {
        return -EINVAL;
    }
    
    /* Update current values */
    sensor_data->soil_moisture = soil_moisture;
    sensor_data->light_level = light_level;
    sensor_data->temperature = temperature;
    sensor_data->humidity = humidity;
    sensor_data->air_movement = air_movement;
    sensor_data->timestamp = k_uptime_get() / 1000;
    
    /* Update history arrays */
    static int64_t last_hourly_update = 0;
    int64_t now = sensor_data->timestamp;
    
    /* Only update history once per hour */
    if (last_hourly_update == 0 || now - last_hourly_update >= 3600) {
        /* Soil moisture */
        sensor_data->history[0].values[sensor_data->history[0].index] = soil_moisture;
        sensor_data->history[0].index = (sensor_data->history[0].index + 1) % 24;
        if (sensor_data->history[0].index == 0) {
            sensor_data->history[0].filled = true;
        }
        
        /* Light level */
        sensor_data->history[1].values[sensor_data->history[1].index] = light_level;
        sensor_data->history[1].index = (sensor_data->history[1].index + 1) % 24;
        if (sensor_data->history[1].index == 0) {
            sensor_data->history[1].filled = true;
        }
        
        /* Temperature */
        sensor_data->history[2].values[sensor_data->history[2].index] = temperature;
        sensor_data->history[2].index = (sensor_data->history[2].index + 1) % 24;
        if (sensor_data->history[2].index == 0) {
            sensor_data->history[2].filled = true;
        }
        
        /* Humidity */
        sensor_data->history[3].values[sensor_data->history[3].index] = humidity;
        sensor_data->history[3].index = (sensor_data->history[3].index + 1) % 24;
        if (sensor_data->history[3].index == 0) {
            sensor_data->history[3].filled = true;
        }
        
        /* Air movement */
        sensor_data->history[4].values[sensor_data->history[4].index] = air_movement;
        sensor_data->history[4].index = (sensor_data->history[4].index + 1) % 24;
        if (sensor_data->history[4].index == 0) {
            sensor_data->history[4].filled = true;
        }
        
        last_hourly_update = now;
    }
    
    return 0;
}

/**
 * @brief Analyze plant health based on sensor and habitat data
 * 
 * @param sensor_data Current sensor readings with history
 * @param habitat_data Plant's natural habitat data
 * @param result_out Pointer to store analysis results
 * @return 0 on success, negative errno on failure
 */
int ml_analyze_plant_health(const struct sensor_data_with_history *sensor_data,
                           const struct habitat_data *habitat_data,
                           struct ml_analysis_result *result_out)
{
    if (!sensor_data || !habitat_data || !result_out) {
        return -EINVAL;
    }
    
    /* Prepare input data for the model (15 values total) */
    float model_input[15];
    
    /* Current sensor values (5) */
    model_input[0] = sensor_data->soil_moisture;
    model_input[1] = sensor_data->light_level;
    model_input[2] = sensor_data->temperature;
    model_input[3] = sensor_data->humidity;
    model_input[4] = sensor_data->air_movement;
    
    /* Differences from ideal values (5) */
    model_input[5] = compute_moisture_diff(sensor_data->soil_moisture, habitat_data);
    model_input[6] = compute_light_diff(sensor_data->light_level, habitat_data);
    model_input[7] = compute_temp_diff(sensor_data->temperature, habitat_data);
    model_input[8] = compute_humidity_diff(sensor_data->humidity, habitat_data);
    model_input[9] = 0.0f;  /* No ideal value for air movement */
    
    /* Calculate averages of historical data (5) */
    for (int i = 0; i < 5; i++) {
        float sum = 0.0f;
        int count = 0;
        int history_len = sensor_data->history[i].filled ? 24 : sensor_data->history[i].index;
        
        for (int j = 0; j < history_len; j++) {
            sum += sensor_data->history[i].values[j];
            count++;
        }
        
        model_input[10 + i] = (count > 0) ? (sum / count) : model_input[i];
    }
    
    /* Output buffer for the model */
    float model_output[3]; /* Health classification probabilities */
    
    /* Run inference */
    int ret = tflite_run_inference(&tflite_ctx, model_input, 15, model_output, 3);
    if (ret < 0) {
        LOG_ERR("ML inference failed: %d", ret);
        return ret;
    }
    
    /* Process model outputs */
    /* Find class with highest probability */
    float max_prob = model_output[0];
    int health_class = ML_HEALTH_HEALTHY;
    
    for (int i = 1; i < 3; i++) {
        if (model_output[i] > max_prob) {
            max_prob = model_output[i];
            health_class = i;
        }
    }
    
    /* Check for environmental mismatches */
    result_out->environmental_mismatch.temperature = 
        is_temp_mismatch(sensor_data->temperature, habitat_data);
    
    result_out->environmental_mismatch.humidity = 
        is_humidity_mismatch(sensor_data->humidity, habitat_data);
    
    result_out->environmental_mismatch.soil_moisture = 
        is_moisture_mismatch(sensor_data->soil_moisture, habitat_data);
    
    result_out->environmental_mismatch.light_level = 
        is_light_mismatch(sensor_data->light_level, habitat_data);
    
    /* Fill result structure */
    result_out->health_status = health_class;
    result_out->confidence = max_prob;
    
    /* Generate recommendations based on mismatches */
    generate_recommendations(result_out);
    
    return 0;
}

/**
 * @brief Generate storage key for sensor history
 */
static void generate_history_key(const char *serial_number, char *key_out, size_t key_size)
{
    snprintf(key_out, key_size, "%s%s", SENSOR_HISTORY_KEY_PREFIX, serial_number);
}

/**
 * @brief Save sensor data history to storage
 * 
 * @param serial_number Device serial number
 * @param sensor_data Sensor data with history
 * @return 0 on success, negative errno on failure
 */
int ml_save_sensor_history(const char *serial_number,
                         const struct sensor_data_with_history *sensor_data)
{
    char key[SENSOR_HISTORY_KEY_MAX];
    generate_history_key(serial_number, key, sizeof(key));
    
    int ret = storage_save_value(key, sensor_data, sizeof(struct sensor_data_with_history));
    if (ret < 0) {
        LOG_ERR("Failed to save sensor history: %d", ret);
        return ret;
    }
    
    return 0;
}

/**
 * @brief Load sensor data history from storage
 * 
 * @param serial_number Device serial number
 * @param sensor_data Pointer to store sensor data with history
 * @return 0 on success, negative errno on failure
 */
int ml_load_sensor_history(const char *serial_number,
                         struct sensor_data_with_history *sensor_data)
{
    char key[SENSOR_HISTORY_KEY_MAX];
    generate_history_key(serial_number, key, sizeof(key));
    
    size_t data_size = sizeof(struct sensor_data_with_history);
    int ret = storage_load_value(key, sensor_data, &data_size);
    if (ret < 0) {
        /* Initialize fresh history if not found */
        if (ret == -ENOENT) {
            memset(sensor_data, 0, sizeof(struct sensor_data_with_history));
            return 0;
        }
        
        LOG_ERR("Failed to load sensor history: %d", ret);
        return ret;
    }
    
    return 0;
}