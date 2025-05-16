#ifndef SENSORS_H
#define SENSORS_H

/**
 * @brief Initialize sensors
 *
 * @return 0 on success, negative errno on failure
 */
int sensors_init(void);

/**
 * @brief Read sensor values
 *
 * @param soil_moisture_out Pointer to store soil moisture value (0-100%)
 * @param light_level_out Pointer to store light level value (0-100%)
 * @param temperature_out Pointer to store temperature value (°C)
 * @param humidity_out Pointer to store humidity value (0-100%)
 * @param air_movement_out Pointer to store air movement value (relative value)
 * @return 0 on success, negative errno on failure
 */
int sensors_read(float *soil_moisture_out, float *light_level_out,
                float *temperature_out, float *humidity_out,
                float *air_movement_out);

#endif /* SENSORS_H */