#ifndef WATER_ANALYSIS_H
#define WATER_ANALYSIS_H

#include <stdint.h>
#include <stdbool.h>

/* Water analysis period */
#define WATER_ANALYSIS_HISTORY_DAYS 7
#define SAMPLES_PER_DAY 24 // One per hour
#define WATER_HISTORY_SIZE (WATER_ANALYSIS_HISTORY_DAYS * SAMPLES_PER_DAY)

/* Water consumption pattern */
struct water_consumption_pattern {
    /* Daily consumption rate (% moisture loss per day) */
    float daily_consumption_rate;
    
    /* Is rate declining (plant may be dormant/less active) */
    bool declining_consumption;
    
    /* Next watering prediction */
    int64_t next_watering_timestamp;
    
    /* Confidence in prediction (0-100%) */
    float prediction_confidence;
    
    /* Moisture history used for prediction */
    struct {
        float moisture[WATER_HISTORY_SIZE];
        int64_t timestamps[WATER_HISTORY_SIZE];
        int index;
        bool filled;
    } history;
};

/**
 * @brief Initialize water analysis module
 * 
 * @return 0 on success, negative errno on failure
 */
int water_analysis_init(void);

/**
 * @brief Add moisture reading to history
 * 
 * @param moisture Current moisture reading
 * @param timestamp Current timestamp
 * @return 0 on success, negative errno on failure
 */
int water_analysis_add_reading(float moisture, int64_t timestamp);

/**
 * @brief Analyze water consumption pattern
 * 
 * @param pattern_out Pointer to pattern structure to fill
 * @param current_moisture Current moisture level
 * @param moisture_threshold Threshold at which watering is needed
 * @return 0 on success, negative errno on failure
 */
int water_analysis_predict_watering(struct water_consumption_pattern *pattern_out,
                                   float current_moisture,
                                   float moisture_threshold);

/**
 * @brief Save water analysis data to storage
 * 
 * @param serial_number Device serial number
 * @return 0 on success, negative errno on failure
 */
int water_analysis_save(const char *serial_number);

/**
 * @brief Load water analysis data from storage
 * 
 * @param serial_number Device serial number
 * @return 0 on success, negative errno on failure
 */
int water_analysis_load(const char *serial_number);

#endif /* WATER_ANALYSIS_H */