#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/net_if.h>
#include <zephyr/settings/settings.h>
#include <zephyr/random/rand32.h>
#include <string.h>

#include "serial_number.h"
#include "storage.h"

LOG_MODULE_REGISTER(serial_number, CONFIG_LOG_DEFAULT_LEVEL);

#define SERIAL_NUMBER_KEY "serial_number"

static char serial_number[33];
static bool serial_number_initialized = false;

/**
 * @brief Generate a serial number using MAC address
 *
 * @param serial_out Buffer to store the generated serial number
 * @param serial_len Length of the output buffer
 * @return 0 on success, negative errno on failure
 */
static int generate_serial_number(char *serial_out, size_t serial_len)
{
    uint8_t mac[6];
    uint32_t random_value;
    int ret;
    
    /* Get MAC address from system */
    struct net_if *iface = net_if_get_default();
    if (!iface) {
        LOG_ERR("Failed to get default network interface");
        return -ENODEV;
    }
    
    /* Get MAC address */
    if (net_if_get_link_addr(iface)->len != 6) {
        LOG_ERR("Invalid MAC address length");
        return -EINVAL;
    }
    
    memcpy(mac, net_if_get_link_addr(iface)->addr, 6);
    
    /* Get a random value for additional entropy */
    random_value = sys_rand32_get();
    
    /* Generate serial number in format: GROW-XXXXXXXXXXXX */
    snprintf(serial_out, serial_len, "GROW-%02X%02X%02X%02X%02X%02X%08X",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], random_value);
    
    /* Save the serial number to flash */
    ret = storage_save_value(SERIAL_NUMBER_KEY, serial_out, strlen(serial_out));
    if (ret < 0) {
        LOG_ERR("Failed to save serial number: %d", ret);
        return ret;
    }
    
    return 0;
}

/**
 * @brief Load or generate serial number
 *
 * @param serial_out Buffer to store the serial number
 * @param serial_len Length of the output buffer
 * @return 0 on success, negative errno on failure
 */
int serial_number_init(char *serial_out, size_t serial_len)
{
    int ret;
    size_t len = serial_len;
    
    if (serial_number_initialized) {
        strncpy(serial_out, serial_number, serial_len);
        serial_out[serial_len - 1] = '\0';
        return 0;
    }
    
    /* Try to load serial number from storage */
    ret = storage_load_value(SERIAL_NUMBER_KEY, serial_out, &len);
    
    /* If not found or error, generate a new one */
    if (ret < 0) {
        LOG_INF("Serial number not found, generating new one");
        ret = generate_serial_number(serial_out, serial_len);
        if (ret < 0) {
            LOG_ERR("Failed to generate serial number: %d", ret);
            return ret;
        }
    } else {
        LOG_INF("Loaded existing serial number");
    }
    
    /* Store in static buffer for future use */
    strncpy(serial_number, serial_out, sizeof(serial_number));
    serial_number[sizeof(serial_number) - 1] = '\0';
    serial_number_initialized = true;
    
    return 0;
}