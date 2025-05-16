#ifndef ML_ANALYSIS_H
#define ML_ANALYSIS_H

#include "habitat_data.h"

/* Plant health status definitions */
#define ML_HEALTH_HEALTHY 0
#define ML_HEALTH_STRESSED 1
#define ML_HEALTH_CRITICAL 2

/* Sensor data structure with history */
struct sensor_data_with_history {
    /* Current values */
    float soil_moisture;
    float light_level;
    float temperature;
    float humidity;
    float air_movement;
    int64_t timestamp;
    
    /* Historical data for trends */
    struct {
        float values[24];  /* Last 24 hours (one per hour) */
        int index;         /* Current position in circular buffer */
        bool filled;       /* Whether the buffer has been filled once */
    } history[5];          /* One history array per sensor type */
};

/* Analysis result structure */
struct ml_analysis_result {
    int health_status;  /* HEALTHY, STRESSED, CRITICAL */
    float confidence;
    struct {
        bool temperature;
        bool humidity;
        bool soil_moisture;
        bool light_level;
    } environmental_mismatch;
    char recommendation[256];
};

/**
 * @brief Initialize ML analysis module
 * 
 * @return 0 on success, negative errno on failure
 */
int ml_analysis_init(void);

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
                         float air_movement);

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
                           struct ml_analysis_result *result_out);

/**
 * @brief Save sensor data history to storage
 * 
 * @param serial_number Device serial number
 * @param sensor_data Sensor data with history
 * @return 0 on success, negative errno on failure
 */
int ml_save_sensor_history(const char *serial_number,
                         const struct sensor_data_with_history *sensor_data);

/**
 * @brief Load sensor data history from storage
 * 
 * @param serial_number Device serial number
 * @param sensor_data Pointer to store sensor data with history
 * @return 0 on success, negative errno on failure
 */
int ml_load_sensor_history(const char *serial_number,
                         struct sensor_data_with_history *sensor_data);

#endif /* ML_ANALYSIS_H */