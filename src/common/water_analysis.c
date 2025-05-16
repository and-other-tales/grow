#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include <errno.h>
#include <math.h>

#include "water_analysis.h"
#include "../storage.h"

LOG_MODULE_REGISTER(water_analysis, CONFIG_LOG_DEFAULT_LEVEL);

/* Static buffer for water pattern analysis */
static struct water_consumption_pattern water_pattern;

/**
 * @brief Initialize water analysis module
 * 
 * @return 0 on success, negative errno on failure
 */
int water_analysis_init(void)
{
    /* Clear water pattern data */
    memset(&water_pattern, 0, sizeof(water_pattern));
    
    LOG_INF("Water analysis module initialized");
    return 0;
}

/**
 * @brief Add moisture reading to history
 * 
 * @param moisture Current moisture reading
 * @param timestamp Current timestamp
 * @return 0 on success, negative errno on failure
 */
int water_analysis_add_reading(float moisture, int64_t timestamp)
{
    /* Add to circular buffer */
    water_pattern.history.moisture[water_pattern.history.index] = moisture;
    water_pattern.history.timestamps[water_pattern.history.index] = timestamp;
    
    /* Update index */
    water_pattern.history.index = (water_pattern.history.index + 1) % WATER_HISTORY_SIZE;
    
    /* Mark as filled if we've gone all the way around */
    if (water_pattern.history.index == 0) {
        water_pattern.history.filled = true;
    }
    
    return 0;
}

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
                                   float moisture_threshold)
{
    if (!pattern_out) {
        return -EINVAL;
    }
    
    /* Copy current pattern data */
    memcpy(pattern_out, &water_pattern, sizeof(struct water_consumption_pattern));
    
    /* Only make predictions if we have enough data */
    if (!water_pattern.history.filled && water_pattern.history.index < 48) {
        /* Need at least 48 samples (2 days) */
        LOG_WRN("Insufficient data for water prediction");
        pattern_out->next_watering_timestamp = 0;
        pattern_out->prediction_confidence = 0.0f;
        return 0;
    }
    
    /* Find trend in moisture data - looking for consistent decline pattern */
    float total_decline = 0.0f;
    int count = 0;
    int usable_samples = water_pattern.history.filled ? 
                        WATER_HISTORY_SIZE : water_pattern.history.index;
    
    /* Start with the most recent data (working backward from current index) */
    int start_idx = (water_pattern.history.index + WATER_HISTORY_SIZE - 1) % WATER_HISTORY_SIZE;
    int prev_idx = (start_idx + WATER_HISTORY_SIZE - 1) % WATER_HISTORY_SIZE;
    
    /* Calculate average decline per hour */
    for (int i = 0; i < usable_samples - 1; i++) {
        int current_idx = (start_idx - i + WATER_HISTORY_SIZE) % WATER_HISTORY_SIZE;
        int prev_idx = (current_idx - 1 + WATER_HISTORY_SIZE) % WATER_HISTORY_SIZE;
        
        /* Skip if timestamps are not sequential or if moisture increased (watering event) */
        int64_t time_diff = water_pattern.history.timestamps[current_idx] - 
                         water_pattern.history.timestamps[prev_idx];
        float moisture_diff = water_pattern.history.moisture[prev_idx] - 
                            water_pattern.history.moisture[current_idx];
        
        if (time_diff > 0 && time_diff < 7200 && moisture_diff > 0) {
            /* Valid sample - moisture is decreasing */
            total_decline += moisture_diff;
            count++;
        } else if (moisture_diff < -5.0f) {
            /* Significant increase detected - likely a watering event */
            /* Stop the analysis at this point */
            break;
        }
    }
    
    /* Calculate average decline rate */
    float hourly_decline_rate = 0.0f;
    if (count > 0) {
        hourly_decline_rate = total_decline / count;
    }
    
    /* Calculate daily consumption rate */
    pattern_out->daily_consumption_rate = hourly_decline_rate * 24;
    
    /* Determine if consumption rate is declining (comparing first and second half) */
    if (count >= 48) { /* If we have at least 2 days of data */
        float first_half_rate = 0.0f;
        float second_half_rate = 0.0f;
        int halfway = count / 2;
        
        /* Calculate rates for each half */
        for (int i = 0; i < halfway; i++) {
            int current_idx = (start_idx - i + WATER_HISTORY_SIZE) % WATER_HISTORY_SIZE;
            int prev_idx = (current_idx - 1 + WATER_HISTORY_SIZE) % WATER_HISTORY_SIZE;
            
            float moisture_diff = water_pattern.history.moisture[prev_idx] - 
                                water_pattern.history.moisture[current_idx];
            if (moisture_diff > 0) {
                second_half_rate += moisture_diff;
            }
        }
        
        for (int i = halfway; i < count; i++) {
            int current_idx = (start_idx - i + WATER_HISTORY_SIZE) % WATER_HISTORY_SIZE;
            int prev_idx = (current_idx - 1 + WATER_HISTORY_SIZE) % WATER_HISTORY_SIZE;
            
            float moisture_diff = water_pattern.history.moisture[prev_idx] - 
                                water_pattern.history.moisture[current_idx];
            if (moisture_diff > 0) {
                first_half_rate += moisture_diff;
            }
        }
        
        second_half_rate /= halfway;
        first_half_rate /= (count - halfway);
        
        /* Determine if declining */
        pattern_out->declining_consumption = (second_half_rate < first_half_rate);
    } else {
        pattern_out->declining_consumption = false;
    }
    
    /* Calculate next watering time based on current moisture and decline rate */
    if (pattern_out->daily_consumption_rate > 0.01f) {
        /* How many hours until we hit threshold */
        float hours_until_threshold = 0;
        
        if (current_moisture > moisture_threshold) {
            hours_until_threshold = (current_moisture - moisture_threshold) / hourly_decline_rate;
        }
        
        /* Set predicted timestamp */
        if (hours_until_threshold > 0) {
            pattern_out->next_watering_timestamp = k_uptime_get() / 1000 + 
                                                 (int64_t)(hours_until_threshold * 3600);
            
            /* Calculate confidence based on data quantity and consistency */
            float data_quantity_factor = fmin(1.0f, (float)count / 72.0f); /* 3 days = full confidence */
            
            /* Calculate variance in decline rate to judge consistency */
            float variance_sum = 0.0f;
            for (int i = 0; i < count; i++) {
                int current_idx = (start_idx - i + WATER_HISTORY_SIZE) % WATER_HISTORY_SIZE;
                int prev_idx = (current_idx - 1 + WATER_HISTORY_SIZE) % WATER_HISTORY_SIZE;
                
                float moisture_diff = water_pattern.history.moisture[prev_idx] - 
                                    water_pattern.history.moisture[current_idx];
                
                if (moisture_diff > 0) {
                    float deviation = moisture_diff - hourly_decline_rate;
                    variance_sum += deviation * deviation;
                }
            }
            
            float variance = count > 0 ? variance_sum / count : 0;
            float stddev = sqrtf(variance);
            float consistency_factor = hourly_decline_rate > 0.001f ? 
                                     fmin(1.0f, 1.0f / (1.0f + 10.0f * stddev / hourly_decline_rate)) : 0;
            
            /* Combine factors for overall confidence */
            pattern_out->prediction_confidence = data_quantity_factor * consistency_factor * 100.0f;
        } else {
            /* Already at or below threshold */
            pattern_out->next_watering_timestamp = k_uptime_get() / 1000;
            pattern_out->prediction_confidence = 100.0f;
        }
    } else {
        /* Insufficient decline rate for prediction */
        pattern_out->next_watering_timestamp = 0;
        pattern_out->prediction_confidence = 0.0f;
    }
    
    LOG_INF("Water analysis - Daily rate: %.2f%%, Next watering: %lld, Confidence: %.1f%%",
           pattern_out->daily_consumption_rate,
           (long long)pattern_out->next_watering_timestamp,
           pattern_out->prediction_confidence);
    
    return 0;
}

/**
 * @brief Save water analysis data to storage
 * 
 * @param serial_number Device serial number
 * @return 0 on success, negative errno on failure
 */
int water_analysis_save(const char *serial_number)
{
    char key[64];
    snprintf(key, sizeof(key), "water/%s", serial_number);
    
    int ret = storage_save_value(key, &water_pattern, sizeof(water_pattern));
    if (ret < 0) {
        LOG_ERR("Failed to save water analysis data: %d", ret);
    }
    
    return ret;
}

/**
 * @brief Load water analysis data from storage
 * 
 * @param serial_number Device serial number
 * @return 0 on success, negative errno on failure
 */
int water_analysis_load(const char *serial_number)
{
    char key[64];
    snprintf(key, sizeof(key), "water/%s", serial_number);
    
    size_t size = sizeof(water_pattern);
    int ret = storage_load_value(key, &water_pattern, &size);
    
    if (ret < 0) {
        LOG_ERR("Failed to load water analysis data: %d", ret);
    } else if (size != sizeof(water_pattern)) {
        LOG_ERR("Invalid water analysis data size: %zu (expected %zu)", 
               size, sizeof(water_pattern));
        ret = -EINVAL;
    }
    
    return ret;
}