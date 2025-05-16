#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "ble.h"
#include "sensors.h"
#include "connectivity.h"
#include "firebase.h"
#include "storage.h"
#include "serial_number.h"
#include "data_cache.h"
#include "button_handler.h"
#include "common/ml_analysis.h"
#include "common/habitat_data.h"
#include "common/plant_analysis.h"
#include "common/water_analysis.h"

LOG_MODULE_REGISTER(main, CONFIG_LOG_DEFAULT_LEVEL);

/* Sensor reading interval (60 seconds) */
#define SENSOR_READ_INTERVAL K_SECONDS(60)

/* Define work item for sensor reading */
static struct k_work_delayable sensor_work;

/* Device information structure */
struct device_info {
    char serial_number[33];
    char plant_name[64];
    char plant_variety[64];
    bool provisioned;
};

static struct device_info dev_info;

/* Sensor data structure */
struct sensor_data {
    float soil_moisture;
    float light_level;
    float temperature;
    float humidity;
    float air_movement;
    int64_t timestamp;
};

static struct sensor_data current_sensor_data;

/* ML analysis result */
static struct ml_analysis_result ml_result;

/* Forward declarations */
static void sensor_work_handler(struct k_work *work);

void main(void)
{
    int ret;
    
    LOG_INF("Grow plant monitor starting...");
    
    /* Initialize storage subsystem */
    ret = storage_init();
    if (ret < 0) {
        LOG_ERR("Failed to initialize storage: %d", ret);
        return;
    }
    
    /* Initialize serial number */
    ret = serial_number_init(dev_info.serial_number, sizeof(dev_info.serial_number));
    if (ret < 0) {
        LOG_ERR("Failed to initialize serial number: %d", ret);
        return;
    }
    LOG_INF("Device serial number: %s", dev_info.serial_number);
    
    /* Load device configuration */
    ret = storage_load_device_config(dev_info.plant_name, sizeof(dev_info.plant_name),
                                    dev_info.plant_variety, sizeof(dev_info.plant_variety),
                                    &dev_info.provisioned);
    
    /* Initialize sensors */
    ret = sensors_init();
    if (ret < 0) {
        LOG_ERR("Failed to initialize sensors: %d", ret);
        return;
    }
    
    /* Initialize connectivity */
    ret = connectivity_init();
    if (ret < 0) {
        LOG_ERR("Failed to initialize connectivity: %d", ret);
        return;
    }
    
    /* Initialize button handler */
    ret = button_handler_init();
    if (ret < 0) {
        LOG_ERR("Failed to initialize button handler: %d", ret);
    }
    
    /* Initialize data cache */
    ret = data_cache_init();
    if (ret < 0) {
        LOG_ERR("Failed to initialize data cache: %d", ret);
    }
    
    /* Load cached data if any */
    ret = data_cache_load(dev_info.serial_number);
    if (ret < 0) {
        LOG_WRN("Failed to load cached data: %d", ret);
    }
    
    /* Initialize water analysis */
    ret = water_analysis_init();
    if (ret < 0) {
        LOG_ERR("Failed to initialize water analysis: %d", ret);
    }
    
    /* Load water analysis data if any */
    ret = water_analysis_load(dev_info.serial_number);
    if (ret < 0) {
        LOG_WRN("Failed to load water analysis data: %d", ret);
    }
    
    /* Initialize plant analysis subsystem */
    ret = plant_analysis_init();
    if (ret < 0) {
        LOG_ERR("Failed to initialize plant analysis: %d", ret);
        return;
    }
    
    /* Initialize BLE for provisioning */
    ret = ble_init(&dev_info.provisioned);
    if (ret < 0) {
        LOG_ERR("Failed to initialize BLE: %d", ret);
        return;
    }
    
    /* Setup sensor work */
    k_work_init_delayable(&sensor_work, sensor_work_handler);
    
    /* If already provisioned, connect to WiFi */
    if (dev_info.provisioned) {
        LOG_INF("Device already provisioned, connecting to network...");
        ret = connectivity_connect();
        if (ret < 0) {
            LOG_ERR("Failed to connect to network: %d", ret);
        }
    } else {
        LOG_INF("Device not provisioned, waiting for BLE provisioning...");
    }
    
    /* Start sensor readings */
    k_work_schedule(&sensor_work, K_NO_WAIT);
    
    /* Main loop */
    while (1) {
        k_sleep(K_FOREVER);
    }
}

/* Handler for sensor readings */
static void sensor_work_handler(struct k_work *work)
{
    int ret;
    struct water_consumption_pattern water_pattern;
    char plant_status[32];
    char mismatch_str[64] = {0};
    
    /* Read sensor data */
    ret = sensors_read(&current_sensor_data.soil_moisture,
                      &current_sensor_data.light_level,
                      &current_sensor_data.temperature,
                      &current_sensor_data.humidity,
                      &current_sensor_data.air_movement);
    
    if (ret < 0) {
        LOG_ERR("Failed to read sensors: %d", ret);
    } else {
        LOG_INF("Sensor readings - Moisture: %.2f%%, Light: %.2f%%, Temp: %.2f°C, Humidity: %.2f%%, Air: %.2f",
               current_sensor_data.soil_moisture,
               current_sensor_data.light_level,
               current_sensor_data.temperature,
               current_sensor_data.humidity,
               current_sensor_data.air_movement);
        
        /* Get current timestamp */
        current_sensor_data.timestamp = k_uptime_get() / 1000;
        
        /* If device is provisioned, perform analysis and send data */
        if (dev_info.provisioned) {
            /* Perform plant analysis */
            ret = plant_analysis_process_reading(
                dev_info.serial_number,
                dev_info.plant_name,
                dev_info.plant_variety,
                current_sensor_data.soil_moisture,
                current_sensor_data.light_level,
                current_sensor_data.temperature,
                current_sensor_data.humidity,
                current_sensor_data.air_movement,
                &ml_result
            );
            
            if (ret < 0) {
                LOG_ERR("Failed to analyze plant health: %d", ret);
            } else {
                LOG_INF("Plant health: %d (Confidence: %.2f)",
                       ml_result.health_status, ml_result.confidence);
                
                /* Get mismatch and status strings */
                plant_analysis_get_mismatch_string(&ml_result, mismatch_str, sizeof(mismatch_str));
                plant_analysis_get_status_string(&ml_result, plant_status, sizeof(plant_status));
                
                /* Update water analysis with new moisture reading */
                water_analysis_add_reading(current_sensor_data.soil_moisture, 
                                         current_sensor_data.timestamp);
                
                /* Analyze water consumption pattern */
                water_analysis_predict_watering(&water_pattern, 
                                              current_sensor_data.soil_moisture, 
                                              30.0f);  /* 30% threshold for watering */
                
                /* Save water analysis data */
                water_analysis_save(dev_info.serial_number);
                
                /* If connected, send data to Firebase */
                if (connectivity_is_connected()) {
                    /* First, try to send any cached data */
                    int cache_count = data_cache_count();
                    if (cache_count > 0) {
                        LOG_INF("Sending %d cached readings to Firebase", cache_count);
                        bool all_sent = true;
                        
                        for (int i = 0; i < cache_count; i++) {
                            struct cached_sensor_reading cached_reading;
                            
                            ret = data_cache_get_reading(i, &cached_reading);
                            if (ret == 0) {
                                ret = firebase_send_sensor_data(
                                    dev_info.serial_number,
                                    cached_reading.soil_moisture,
                                    cached_reading.light_level,
                                    cached_reading.temperature,
                                    cached_reading.humidity,
                                    cached_reading.air_movement,
                                    cached_reading.timestamp,
                                    dev_info.plant_name,
                                    dev_info.plant_variety,
                                    cached_reading.health_status,
                                    cached_reading.env_mismatch,
                                    ml_result.recommendation,
                                    cached_reading.plant_status
                                );
                                
                                if (ret < 0) {
                                    LOG_ERR("Failed to send cached data to Firebase: %d", ret);
                                    all_sent = false;
                                    break;
                                }
                            }
                        }
                        
                        if (all_sent) {
                            /* Successfully sent all cached data */
                            data_cache_clear();
                            data_cache_save(dev_info.serial_number);
                            LOG_INF("All cached data sent and cache cleared");
                        }
                    }
                    
                    /* Send current data */
                    ret = firebase_send_sensor_data(
                        dev_info.serial_number,
                        current_sensor_data.soil_moisture,
                        current_sensor_data.light_level,
                        current_sensor_data.temperature,
                        current_sensor_data.humidity,
                        current_sensor_data.air_movement,
                        current_sensor_data.timestamp,
                        dev_info.plant_name,
                        dev_info.plant_variety,
                        ml_result.health_status,
                        mismatch_str,
                        ml_result.recommendation,
                        plant_status
                    );
                    
                    if (ret < 0) {
                        LOG_ERR("Failed to send data to Firebase: %d", ret);
                    }
                    
                    /* Send water prediction data if confidence is high enough */
                    if (water_pattern.prediction_confidence > 30.0f) {
                        ret = firebase_send_water_prediction(
                            dev_info.serial_number,
                            water_pattern.daily_consumption_rate,
                            water_pattern.next_watering_timestamp,
                            water_pattern.prediction_confidence
                        );
                        
                        if (ret < 0) {
                            LOG_ERR("Failed to send water prediction to Firebase: %d", ret);
                        } else {
                            LOG_INF("Water prediction sent: next watering in %.1f hours",
                                  (water_pattern.next_watering_timestamp - current_sensor_data.timestamp) / 3600.0f);
                        }
                    }
                } else {
                    /* Offline - cache the data */
                    LOG_INF("Device offline, caching sensor reading");
                    ret = data_cache_add_reading(
                        current_sensor_data.soil_moisture,
                        current_sensor_data.light_level,
                        current_sensor_data.temperature,
                        current_sensor_data.humidity,
                        current_sensor_data.air_movement,
                        current_sensor_data.timestamp,
                        ml_result.health_status,
                        mismatch_str,
                        plant_status
                    );
                    
                    if (ret < 0) {
                        LOG_ERR("Failed to cache sensor data: %d", ret);
                    } else {
                        /* Save cache to persistent storage */
                        data_cache_save(dev_info.serial_number);
                        LOG_INF("Sensor data cached successfully");
                    }
                }
            }
        }
    }
    
    /* Check for button press requests */
    if (button_reset_requested()) {
        LOG_INF("Processing soft reset request");
        button_clear_requests();
        sys_reboot(SYS_REBOOT_WARM);
    } else if (button_factory_reset_requested()) {
        LOG_INF("Processing factory reset request");
        button_clear_requests();
        
        /* Clear all data */
        storage_reset_device_config();
        
        /* Reboot */
        sys_reboot(SYS_REBOOT_COLD);
    }
    
    /* Schedule next sensor reading */
    k_work_schedule(&sensor_work, SENSOR_READ_INTERVAL);
}

/* Callback for connectivity status */
void connectivity_status_callback(bool connected)
{
    if (connected) {
        LOG_INF("Network connected");
        
        /* Initialize Firebase */
        int ret = firebase_init();
        if (ret < 0) {
            LOG_ERR("Failed to initialize Firebase: %d", ret);
        }
        
        /* Trigger immediate sensor reading to send data */
        k_work_reschedule(&sensor_work, K_NO_WAIT);
    } else {
        LOG_INF("Network disconnected");
    }
}

/* Callback for provisioning */
void provisioning_complete_callback(const char *wifi_ssid, const char *wifi_password,
                                   const char *plant_name, const char *plant_variety)
{
    LOG_INF("Provisioning complete - SSID: %s, Plant: %s, Variety: %s",
           wifi_ssid, plant_name, plant_variety);
    
    /* Save device configuration */
    int ret = storage_save_device_config(wifi_ssid, wifi_password, 
                                       plant_name, plant_variety);
    if (ret < 0) {
        LOG_ERR("Failed to save device configuration: %d", ret);
        return;
    }
    
    /* Update local info */
    strncpy(dev_info.plant_name, plant_name, sizeof(dev_info.plant_name) - 1);
    strncpy(dev_info.plant_variety, plant_variety, sizeof(dev_info.plant_variety) - 1);
    dev_info.provisioned = true;
    
    /* Connect to network */
    ret = connectivity_connect();
    if (ret < 0) {
        LOG_ERR("Failed to connect to network: %d", ret);
    }
}