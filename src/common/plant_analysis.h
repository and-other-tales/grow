#ifndef PLANT_ANALYSIS_H
#define PLANT_ANALYSIS_H

#include "ml_analysis.h"
#include "habitat_data.h"

/**
 * @brief Initialize plant analysis subsystem
 * 
 * @return 0 on success, negative errno on failure
 */
int plant_analysis_init(void);

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
                                 struct ml_analysis_result *result_out);

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
                                     size_t output_size);

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
                                   size_t output_size);

#endif /* PLANT_ANALYSIS_H */