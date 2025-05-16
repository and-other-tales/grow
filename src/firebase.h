#ifndef FIREBASE_H
#define FIREBASE_H

#include <stdint.h>

/**
 * @brief Initialize Firebase connection
 *
 * @return 0 on success, negative errno on failure
 */
int firebase_init(void);

/**
 * @brief Send sensor data to Firebase
 *
 * @param serial_number Device serial number
 * @param soil_moisture Soil moisture value (0-100%)
 * @param light_level Light level value (0-100%)
 * @param temperature Temperature value (°C)
 * @param humidity Humidity value (0-100%)
 * @param air_movement Air movement value
 * @param timestamp Timestamp of reading
 * @param plant_name Plant name
 * @param plant_variety Plant variety
 * @param health_status Plant health status
 * @param env_mismatch Environmental mismatch flags
 * @param recommendation Recommendations
 * @param plant_status Plant status string
 * @return 0 on success, negative errno on failure
 */
int firebase_send_sensor_data(const char *serial_number,
                             float soil_moisture,
                             float light_level,
                             float temperature,
                             float humidity,
                             float air_movement,
                             int64_t timestamp,
                             const char *plant_name,
                             const char *plant_variety,
                             int health_status, 
                             const char *env_mismatch,
                             const char *recommendation,
                             const char *plant_status);

/**
 * @brief Send water prediction data to Firebase
 *
 * @param serial_number Device serial number
 * @param daily_consumption_rate Daily water consumption rate
 * @param next_watering_timestamp Predicted next watering timestamp
 * @param prediction_confidence Confidence level in prediction
 * @return 0 on success, negative errno on failure
 */
int firebase_send_water_prediction(const char *serial_number,
                                 float daily_consumption_rate,
                                 int64_t next_watering_timestamp,
                                 float prediction_confidence);

#endif /* FIREBASE_H */