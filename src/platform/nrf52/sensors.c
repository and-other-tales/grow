#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <math.h>
#include <string.h>

#include "../../sensors.h"

LOG_MODULE_REGISTER(sensors, CONFIG_LOG_DEFAULT_LEVEL);

/* ADC definitions */
#define ADC_NODE DT_NODELABEL(adc)
#define ADC_RESOLUTION 12
#define ADC_CHANNEL_SOIL 0
#define ADC_CHANNEL_LIGHT 1
#define ADC_CHANNEL_AIR 2

/* DHT22 definitions */
#define DHT_NODE DT_NODELABEL(dht22)

/* ADC channel configuration - nRF52 specific */
static const struct adc_channel_cfg soil_channel_cfg = {
    .gain = ADC_GAIN_1_6,
    .reference = ADC_REF_INTERNAL,
    .acquisition_time = ADC_ACQ_TIME_DEFAULT,
    .channel_id = ADC_CHANNEL_SOIL,
    .input_positive = NRF_SAADC_AIN0,
};

static const struct adc_channel_cfg light_channel_cfg = {
    .gain = ADC_GAIN_1_6,
    .reference = ADC_REF_INTERNAL,
    .acquisition_time = ADC_ACQ_TIME_DEFAULT,
    .channel_id = ADC_CHANNEL_LIGHT,
    .input_positive = NRF_SAADC_AIN1,
};

static const struct adc_channel_cfg air_channel_cfg = {
    .gain = ADC_GAIN_1_6,
    .reference = ADC_REF_INTERNAL,
    .acquisition_time = ADC_ACQ_TIME_DEFAULT,
    .channel_id = ADC_CHANNEL_AIR,
    .input_positive = NRF_SAADC_AIN2,
};

/* ADC sequence configuration */
static const struct adc_sequence sequence = {
    .channels = BIT(0),
    .buffer = NULL,
    .buffer_size = 0,
    .resolution = ADC_RESOLUTION,
};

/* Device pointers */
static const struct device *adc_dev;
static const struct device *dht_dev;

/* Raw ADC buffers */
static int16_t soil_sample_buf;
static int16_t light_sample_buf;
static int16_t air_sample_buf;

/* Calibration values */
#define SOIL_DRY_VALUE 3200   /* ADC value when soil is completely dry */
#define SOIL_WET_VALUE 1400   /* ADC value when soil is saturated */

/**
 * @brief Initialize sensors
 *
 * @return 0 on success, negative errno on failure
 */
int sensors_init(void)
{
    int ret;
    
    /* Get ADC device */
    adc_dev = DEVICE_DT_GET(ADC_NODE);
    if (!device_is_ready(adc_dev)) {
        LOG_ERR("ADC device not ready");
        return -ENODEV;
    }
    
    /* Configure ADC channels */
    ret = adc_channel_setup(adc_dev, &soil_channel_cfg);
    if (ret < 0) {
        LOG_ERR("Failed to setup soil moisture ADC channel: %d", ret);
        return ret;
    }
    
    ret = adc_channel_setup(adc_dev, &light_channel_cfg);
    if (ret < 0) {
        LOG_ERR("Failed to setup light level ADC channel: %d", ret);
        return ret;
    }
    
    ret = adc_channel_setup(adc_dev, &air_channel_cfg);
    if (ret < 0) {
        LOG_ERR("Failed to setup air movement ADC channel: %d", ret);
        return ret;
    }
    
    /* Get DHT22 device */
    dht_dev = DEVICE_DT_GET(DHT_NODE);
    if (!device_is_ready(dht_dev)) {
        LOG_ERR("DHT22 device not ready");
        return -ENODEV;
    }
    
    LOG_INF("Sensors initialized successfully");
    
    return 0;
}

/**
 * @brief Read soil moisture from ADC
 *
 * @param value_out Pointer to store soil moisture value (0-100%)
 * @return 0 on success, negative errno on failure
 */
static int read_soil_moisture(float *value_out)
{
    int ret;
    struct adc_sequence sequence_soil = sequence;
    sequence_soil.channels = BIT(ADC_CHANNEL_SOIL);
    sequence_soil.buffer = &soil_sample_buf;
    sequence_soil.buffer_size = sizeof(soil_sample_buf);
    
    ret = adc_read(adc_dev, &sequence_soil);
    if (ret < 0) {
        return ret;
    }
    
    /* nRF52-specific: Convert raw ADC value to mV */
    int32_t mv_value = soil_sample_buf;
    ret = adc_raw_to_millivolts(adc_get_ref_internal(adc_dev),
                              soil_channel_cfg.gain,
                              ADC_RESOLUTION,
                              &mv_value);
    if (ret < 0) {
        return ret;
    }
    
    /* Convert millivolts to moisture percentage (0-100%) */
    /* Note: This needs calibration for specific soil sensor */
    /* Assuming 1000mV = 0% moisture, 3000mV = 100% moisture */
    float moisture = (mv_value - 1000) / 20.0f;
    
    /* Clamp value to valid range */
    if (moisture < 0.0) {
        moisture = 0.0;
    } else if (moisture > 100.0) {
        moisture = 100.0;
    }
    
    *value_out = moisture;
    return 0;
}

/**
 * @brief Read light level from ADC
 *
 * @param value_out Pointer to store light level value (0-100%)
 * @return 0 on success, negative errno on failure
 */
static int read_light_level(float *value_out)
{
    int ret;
    struct adc_sequence sequence_light = sequence;
    sequence_light.channels = BIT(ADC_CHANNEL_LIGHT);
    sequence_light.buffer = &light_sample_buf;
    sequence_light.buffer_size = sizeof(light_sample_buf);
    
    ret = adc_read(adc_dev, &sequence_light);
    if (ret < 0) {
        return ret;
    }
    
    /* nRF52-specific: Convert raw ADC value to mV */
    int32_t mv_value = light_sample_buf;
    ret = adc_raw_to_millivolts(adc_get_ref_internal(adc_dev),
                              light_channel_cfg.gain,
                              ADC_RESOLUTION,
                              &mv_value);
    if (ret < 0) {
        return ret;
    }
    
    /* Convert millivolts to light percentage (0-100%) */
    /* Lower voltage = more light, assuming 3300mV max */
    float light = 100.0 - ((float)mv_value * 100.0) / 3300.0;
    
    /* Clamp value to valid range */
    if (light < 0.0) {
        light = 0.0;
    } else if (light > 100.0) {
        light = 100.0;
    }
    
    *value_out = light;
    return 0;
}

/**
 * @brief Read air movement from ADC
 *
 * @param value_out Pointer to store air movement value
 * @return 0 on success, negative errno on failure
 */
static int read_air_movement(float *value_out)
{
    int ret;
    struct adc_sequence sequence_air = sequence;
    sequence_air.channels = BIT(ADC_CHANNEL_AIR);
    sequence_air.buffer = &air_sample_buf;
    sequence_air.buffer_size = sizeof(air_sample_buf);
    
    ret = adc_read(adc_dev, &sequence_air);
    if (ret < 0) {
        return ret;
    }
    
    /* nRF52-specific: Convert raw ADC value to mV */
    int32_t mv_value = air_sample_buf;
    ret = adc_raw_to_millivolts(adc_get_ref_internal(adc_dev),
                              air_channel_cfg.gain,
                              ADC_RESOLUTION,
                              &mv_value);
    if (ret < 0) {
        return ret;
    }
    
    /* Convert millivolts to relative air movement value */
    /* Simple mapping to 0-100 scale */
    *value_out = mv_value / 33.0f;
    
    return 0;
}

/**
 * @brief Read temperature and humidity from DHT22
 *
 * @param temperature_out Pointer to store temperature value
 * @param humidity_out Pointer to store humidity value
 * @return 0 on success, negative errno on failure
 */
static int read_temp_humidity(float *temperature_out, float *humidity_out)
{
    int ret;
    struct sensor_value temp, humidity;
    
    /* Fetch sample from DHT22 */
    ret = sensor_sample_fetch(dht_dev);
    if (ret < 0) {
        LOG_ERR("Failed to fetch DHT22 sample: %d", ret);
        return ret;
    }
    
    /* Get temperature */
    ret = sensor_channel_get(dht_dev, SENSOR_CHAN_AMBIENT_TEMP, &temp);
    if (ret < 0) {
        LOG_ERR("Failed to get temperature: %d", ret);
        return ret;
    }
    
    /* Get humidity */
    ret = sensor_channel_get(dht_dev, SENSOR_CHAN_HUMIDITY, &humidity);
    if (ret < 0) {
        LOG_ERR("Failed to get humidity: %d", ret);
        return ret;
    }
    
    /* Convert to float */
    *temperature_out = sensor_value_to_double(&temp);
    *humidity_out = sensor_value_to_double(&humidity);
    
    return 0;
}

/**
 * @brief Read all sensor values
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
                float *air_movement_out)
{
    int ret;
    
    /* Read soil moisture */
    ret = read_soil_moisture(soil_moisture_out);
    if (ret < 0) {
        LOG_ERR("Failed to read soil moisture: %d", ret);
        return ret;
    }
    
    /* Read light level */
    ret = read_light_level(light_level_out);
    if (ret < 0) {
        LOG_ERR("Failed to read light level: %d", ret);
        return ret;
    }
    
    /* Read air movement */
    ret = read_air_movement(air_movement_out);
    if (ret < 0) {
        LOG_ERR("Failed to read air movement: %d", ret);
        return ret;
    }
    
    /* Read temperature and humidity */
    ret = read_temp_humidity(temperature_out, humidity_out);
    if (ret < 0) {
        LOG_ERR("Failed to read temp and humidity: %d", ret);
        return ret;
    }
    
    return 0;
}