# grow
Grow Repository Source (ZephyrOS for nrf52840 & ESP32S3/C6)
=======
# Grow - Smart Plant Monitoring System

A ZephyrOS-based plant monitoring system that tracks soil moisture, light levels, temperature, humidity, and air movement. The system uses TensorFlow Lite to analyze plant health and predict watering needs.

## Features

- **Real-time plant monitoring**:
  - Soil moisture level (capacitive sensor)
  - Light level (photoresistor)
  - Temperature and humidity (DHT22)
  - Air movement detection

- **Intelligent Plant Analysis**:
  - ML-based plant health monitoring using TensorFlow Lite
  - Analysis of environmental conditions against plant's natural habitat
  - Identification of environmental mismatches
  - Water consumption analysis and watering prediction
  - Plant status tracking and recommendations

- **Multi-platform Support**:
  - ESP32S3
  - ESP32C6
  - Nordic nRF52840

- **Offline Operation**:
  - Data caching when offline
  - Automatic upload of cached data when connection is restored
  - WiFi reconnection logic with automatic reprovisioning

- **Automated data collection**:
  - Sensor readings every 60 seconds
  - Data stored in Firebase Firestore

- **Easy device setup**:
  - BLE provisioning for WiFi credentials
  - Persistent configuration storage in flash
  - Unique device identification with serial number

- **User Controls**:
  - Double-press button for device restart
  - Long-press (5+ seconds) for factory reset

## Hardware Requirements

- ESP32S3/ESP32C6 or nRF52840 development board
- Capacitive soil moisture sensor
- Photoresistor for light sensing
- DHT22 temperature and humidity sensor
- Resistor for air movement detection
- Button for device control
- LED for visual feedback
- Connection wires

## Wiring Instructions

- **Soil Moisture Sensor**: Connect analog output to ADC channel 0
- **Photoresistor**: Connect to ADC channel 1 via voltage divider
- **DHT22**: Connect data pin to GPIO21 (ESP32) or GPIO13 (nRF52)
- **Air Movement Sensor**: Connect to ADC channel 2
- **Button**: Connect to GPIO as defined in board overlay
- **LED**: Connect to GPIO as defined in board overlay

## Building and Flashing

Make sure you have Zephyr SDK installed and properly set up.

1. Set up Zephyr environment:
   ```bash
   source <zephyr-sdk-path>/zephyr-env.sh
   ```

2. Build for ESP32S3:
   ```bash
   west build -b esp32s3_devkitm
   ```

   Or for ESP32C6:
   ```bash
   west build -b esp32c6_devkitc
   ```

   Or for nRF52840:
   ```bash
   west build -b nrf52840dk_nrf52840
   ```

3. Flash to your device:
   ```bash
   west flash
   ```

## Setup Process

1. **First Boot**:
   - Device generates a unique serial number
   - Enters BLE provisioning mode

2. **Provisioning via Mobile App**:
   - Connect to the device via BLE
   - Send WiFi credentials
   - Set plant name and variety
   - Apply configuration

3. **Normal Operation**:
   - Device connects to WiFi
   - Begins sensor readings and analysis
   - Stores data in Firebase Firestore

## Data Analysis

The system performs several types of analysis:

1. **Environmental Matching**:
   - Fetches natural habitat data for the plant variety
   - Compares current conditions with ideal conditions
   - Identifies mismatches in temperature, humidity, light, and moisture

2. **Water Consumption Analysis**:
   - Tracks soil moisture levels over time
   - Calculates daily water consumption rate
   - Predicts when the plant will need watering
   - Provides watering recommendations with confidence level

3. **Plant Health Status**:
   - Classifies plant health status (Healthy, Stressed, Critical)
   - Updates plantStatus field in Firestore
   - Generates specific recommendations for improving conditions

## Firebase Data Structure

- `/plants/{serialNumber}` - Main sensor data document
  - soilMoisture, lightLevel, temperature, humidity, airMovement
  - healthStatus, environmentalMismatch, recommendation, plantStatus
  - timestamp, plantName, plantVariety

- `/plants/{serialNumber}/waterPrediction/current` - Water prediction data
  - dailyConsumptionRate - Rate of water loss per day
  - nextWateringTime - Predicted time for next watering
  - predictionConfidence - Confidence level in prediction

## Button Controls

- **Double Press**: Soft restart of the device
- **Hold for 5+ Seconds**: Factory reset (LED will blink twice)

## Mobile App Integration

The device is designed to be provisioned by a companion Flutter mobile app (not included). The app should implement the following BLE characteristics:

- WiFi SSID (write)
- WiFi Password (write)
- Plant Name (write)
- Plant Variety (write)
- Apply Configuration (write)
- Device Info (read)

## Offline Operation

The device implements robust offline operation:

1. When WiFi connection is lost, the device:
   - Attempts to reconnect (3 retry attempts)
   - If reconnection fails, it enters reprovisioning mode
   - Continues to make periodic reconnection attempts each hour

2. While offline:
   - Continues to collect and analyze sensor data
   - Caches data in persistent storage
   - Maintains water consumption analysis

3. When connection is restored:
   - Uploads all cached data to Firebase
   - Clears the cache once upload is complete
   - Resumes normal online operation

## License

