#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/logging/log.h>
#include <stdbool.h>
#include <string.h>

#include "ble.h"
#include "serial_number.h"

LOG_MODULE_REGISTER(ble, CONFIG_LOG_DEFAULT_LEVEL);

/* Define UUIDs for our custom service and characteristics */
#define GROW_SERVICE_UUID \
    BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef0)
    
#define WIFI_SSID_CHAR_UUID \
    BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef1)
    
#define WIFI_PASSWORD_CHAR_UUID \
    BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef2)
    
#define PLANT_NAME_CHAR_UUID \
    BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef3)
    
#define PLANT_VARIETY_CHAR_UUID \
    BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef4)
    
#define APPLY_CONFIG_CHAR_UUID \
    BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef5)
    
#define DEVICE_INFO_CHAR_UUID \
    BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef6)

/* Maximum length for each characteristic */
#define MAX_WIFI_SSID_LEN 32
#define MAX_WIFI_PASSWORD_LEN 64
#define MAX_PLANT_NAME_LEN 64
#define MAX_PLANT_VARIETY_LEN 64
#define MAX_DEVICE_INFO_LEN 128

/* Static buffers for characteristic data */
static char wifi_ssid[MAX_WIFI_SSID_LEN + 1];
static char wifi_password[MAX_WIFI_PASSWORD_LEN + 1];
static char plant_name[MAX_PLANT_NAME_LEN + 1];
static char plant_variety[MAX_PLANT_VARIETY_LEN + 1];
static char device_info[MAX_DEVICE_INFO_LEN + 1] = "GrowSense Plant Monitor";
static uint8_t apply_config_value;

/* Pointer to provisioning status */
static bool *device_provisioned;

/* Forward declaration of provisioning callback */
extern void provisioning_complete_callback(const char *wifi_ssid, const char *wifi_password,
                                          const char *plant_name, const char *plant_variety);

/* Static BLE connection */
static struct bt_conn *current_conn;
static bool ble_advertising = false;

/* Connection callbacks */
static void connected(struct bt_conn *conn, uint8_t err)
{
    if (err) {
        LOG_ERR("Connection failed (err %u)", err);
        return;
    }

    LOG_INF("Connected");
    current_conn = bt_conn_ref(conn);
    ble_advertising = false;
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    LOG_INF("Disconnected (reason %u)", reason);

    if (current_conn) {
        bt_conn_unref(current_conn);
        current_conn = NULL;
    }
}

static struct bt_conn_cb conn_callbacks = {
    .connected = connected,
    .disconnected = disconnected,
};

/* WIFI SSID characteristic write callback */
static ssize_t write_wifi_ssid(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                               const void *buf, uint16_t len, uint16_t offset,
                               uint8_t flags)
{
    if (offset + len > MAX_WIFI_SSID_LEN) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
    }

    memcpy(wifi_ssid + offset, buf, len);
    wifi_ssid[offset + len] = '\0';

    LOG_INF("WIFI SSID set to: %s", wifi_ssid);

    return len;
}

/* WIFI Password characteristic write callback */
static ssize_t write_wifi_password(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                                  const void *buf, uint16_t len, uint16_t offset,
                                  uint8_t flags)
{
    if (offset + len > MAX_WIFI_PASSWORD_LEN) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
    }

    memcpy(wifi_password + offset, buf, len);
    wifi_password[offset + len] = '\0';

    LOG_INF("WIFI Password set");

    return len;
}

/* Plant Name characteristic write callback */
static ssize_t write_plant_name(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                               const void *buf, uint16_t len, uint16_t offset,
                               uint8_t flags)
{
    if (offset + len > MAX_PLANT_NAME_LEN) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
    }

    memcpy(plant_name + offset, buf, len);
    plant_name[offset + len] = '\0';

    LOG_INF("Plant Name set to: %s", plant_name);

    return len;
}

/* Plant Variety characteristic write callback */
static ssize_t write_plant_variety(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                                  const void *buf, uint16_t len, uint16_t offset,
                                  uint8_t flags)
{
    if (offset + len > MAX_PLANT_VARIETY_LEN) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
    }

    memcpy(plant_variety + offset, buf, len);
    plant_variety[offset + len] = '\0';

    LOG_INF("Plant Variety set to: %s", plant_variety);

    return len;
}

/* Apply Config characteristic write callback */
static ssize_t write_apply_config(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                                 const void *buf, uint16_t len, uint16_t offset,
                                 uint8_t flags)
{
    if (offset + len > sizeof(apply_config_value)) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
    }

    memcpy(&apply_config_value + offset, buf, len);

    LOG_INF("Apply Config triggered with value: %u", apply_config_value);

    if (apply_config_value == 1) {
        /* Validate configuration data */
        if (strlen(wifi_ssid) > 0 && strlen(wifi_password) > 0) {
            LOG_INF("Configuration valid, applying...");
            
            /* Update provisioning status */
            *device_provisioned = true;
            
            /* Call provisioning callback */
            provisioning_complete_callback(wifi_ssid, wifi_password, plant_name, plant_variety);
        } else {
            LOG_ERR("Invalid configuration, SSID and password are required");
        }
    }

    return len;
}

/* Device Info characteristic read callback */
static ssize_t read_device_info(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                               void *buf, uint16_t len, uint16_t offset)
{
    char *value = attr->user_data;

    return bt_gatt_attr_read(conn, attr, buf, len, offset, value, strlen(value));
}

/* Define our GATT service */
BT_GATT_SERVICE_DEFINE(grow_svc,
    BT_GATT_PRIMARY_SERVICE(BT_UUID_DECLARE_128(GROW_SERVICE_UUID)),
    
    BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_128(DEVICE_INFO_CHAR_UUID),
                          BT_GATT_CHRC_READ,
                          BT_GATT_PERM_READ,
                          read_device_info, NULL, device_info),
                          
    BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_128(WIFI_SSID_CHAR_UUID),
                          BT_GATT_CHRC_WRITE,
                          BT_GATT_PERM_WRITE,
                          NULL, write_wifi_ssid, wifi_ssid),
                          
    BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_128(WIFI_PASSWORD_CHAR_UUID),
                          BT_GATT_CHRC_WRITE,
                          BT_GATT_PERM_WRITE,
                          NULL, write_wifi_password, wifi_password),
                          
    BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_128(PLANT_NAME_CHAR_UUID),
                          BT_GATT_CHRC_WRITE,
                          BT_GATT_PERM_WRITE,
                          NULL, write_plant_name, plant_name),
                          
    BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_128(PLANT_VARIETY_CHAR_UUID),
                          BT_GATT_CHRC_WRITE,
                          BT_GATT_PERM_WRITE,
                          NULL, write_plant_variety, plant_variety),
                          
    BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_128(APPLY_CONFIG_CHAR_UUID),
                          BT_GATT_CHRC_WRITE,
                          BT_GATT_PERM_WRITE,
                          NULL, write_apply_config, &apply_config_value),
);

/* Advertising data */
static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA_BYTES(BT_DATA_UUID128_ALL, GROW_SERVICE_UUID),
};

/* Scan response data */
static const struct bt_data sd[] = {
    BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

/**
 * @brief Initialize BLE subsystem
 *
 * @param provisioned Pointer to device provisioning state
 * @return 0 on success, negative errno on failure
 */
int ble_init(bool *provisioned)
{
    int err;
    
    if (!provisioned) {
        return -EINVAL;
    }
    
    device_provisioned = provisioned;
    
    /* Initialize Bluetooth */
    err = bt_enable(NULL);
    if (err) {
        LOG_ERR("Bluetooth init failed (err %d)", err);
        return err;
    }

    LOG_INF("Bluetooth initialized");
    
    /* Register connection callbacks */
    bt_conn_cb_register(&conn_callbacks);

    /* Set device name from serial number */
    char serial[33];
    err = serial_number_init(serial, sizeof(serial));
    if (err == 0) {
        snprintf(device_info, sizeof(device_info), "GrowSense %s", serial);
    }
    
    /* Start advertising only if not provisioned */
    if (!(*provisioned)) {
        err = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
        if (err) {
            LOG_ERR("Advertising failed to start (err %d)", err);
            return err;
        }
        LOG_INF("Advertising started");
        ble_advertising = true;
    }

    return 0;
}

/**
 * @brief Restart BLE advertising for provisioning
 *
 * Called when device needs to be re-provisioned
 * 
 * @return 0 on success, negative errno on failure
 */
int ble_restart_advertising(void)
{
    int err;
    
    /* Stop advertising if already running */
    if (ble_advertising) {
        bt_le_adv_stop();
        ble_advertising = false;
    }
    
    /* Start advertising */
    err = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
    if (err) {
        LOG_ERR("Advertising failed to restart (err %d)", err);
        return err;
    }
    
    LOG_INF("Re-provisioning mode - BLE advertising restarted");
    ble_advertising = true;
    
    return 0;
}