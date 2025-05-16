cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(grow)

# Platform-specific source files
if(CONFIG_SOC_ESP32S3 OR CONFIG_SOC_ESP32C6)
  set(PLATFORM_DIR "platform/esp32")
elseif(CONFIG_SOC_NRF52840)
  set(PLATFORM_DIR "platform/nrf52")
else()
  message(FATAL_ERROR "Unsupported platform")
endif()

# Common sources
set(COMMON_SOURCES
  src/main.c
  src/ble.c
  src/storage.c
  src/serial_number.c
  src/common/ml_analysis.c
  src/common/habitat_data.c
  src/common/plant_analysis.c
)

# Platform-specific sources
set(PLATFORM_SOURCES
  src/${PLATFORM_DIR}/sensors.c
  src/${PLATFORM_DIR}/connectivity.c
  src/${PLATFORM_DIR}/tflite_platform.c
)

# Add TensorFlow Lite sources based on platform
if(CONFIG_SOC_ESP32S3 OR CONFIG_SOC_ESP32C6)
  list(APPEND PLATFORM_SOURCES src/${PLATFORM_DIR}/firebase.c)
endif()

# Include directories
zephyr_include_directories(
  src
  src/common
  src/${PLATFORM_DIR}
)

# Target sources
target_sources(app PRIVATE
  ${COMMON_SOURCES}
  ${PLATFORM_SOURCES}
)