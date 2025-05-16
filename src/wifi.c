#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_core.h>
#include <zephyr/net/net_context.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/wifi_mgmt.h>
#include <string.h>

#include "wifi.h"
#include "storage.h"

LOG_MODULE_REGISTER(wifi, CONFIG_LOG_DEFAULT_LEVEL);

/* WiFi connection definitions */
#define WIFI_SSID_KEY "wifi/ssid"
#define WIFI_PASSWORD_KEY "wifi/password"
#define MAX_WIFI_SSID_LEN 32
#define MAX_WIFI_PSK_LEN 64

/* WiFi status */
static bool is_connected;
static struct net_if *iface;
static struct net_mgmt_event_callback wifi_cb;

/* WiFi credentials */
static char wifi_ssid[MAX_WIFI_SSID_LEN + 1];
static char wifi_psk[MAX_WIFI_PSK_LEN + 1];

/**
 * @brief WiFi management event handler
 */
static void wifi_mgmt_event_handler(struct net_mgmt_event_callback *cb,
                                   uint32_t mgmt_event, struct net_if *iface)
{
    switch (mgmt_event) {
    case NET_EVENT_WIFI_CONNECT_RESULT:
        LOG_INF("WiFi connected");
        is_connected = true;
        
        /* Call connection callback */
        wifi_connected_callback();
        break;
        
    case NET_EVENT_WIFI_DISCONNECT_RESULT:
        LOG_INF("WiFi disconnected");
        is_connected = false;
        break;
        
    default:
        break;
    }
}

/**
 * @brief Initialize and connect to WiFi
 *
 * @return 0 on success, negative errno on failure
 */
int wifi_connect(void)
{
    int ret;
    size_t len;
    struct wifi_connect_req_params wifi_params = { 0 };
    
    /* Get WiFi interface */
    iface = net_if_get_default();
    if (!iface) {
        LOG_ERR("No network interface available");
        return -ENODEV;
    }
    
    /* Register event handler */
    net_mgmt_init_event_callback(&wifi_cb, wifi_mgmt_event_handler,
                                (NET_EVENT_WIFI_CONNECT_RESULT |
                                 NET_EVENT_WIFI_DISCONNECT_RESULT));
    net_mgmt_add_event_callback(&wifi_cb);
    
    /* Load WiFi credentials from storage */
    len = sizeof(wifi_ssid) - 1;
    ret = storage_load_value(WIFI_SSID_KEY, wifi_ssid, &len);
    if (ret < 0) {
        LOG_ERR("Failed to load WiFi SSID: %d", ret);
        return ret;
    }
    wifi_ssid[len] = '\0';
    
    len = sizeof(wifi_psk) - 1;
    ret = storage_load_value(WIFI_PASSWORD_KEY, wifi_psk, &len);
    if (ret < 0) {
        LOG_ERR("Failed to load WiFi password: %d", ret);
        return ret;
    }
    wifi_psk[len] = '\0';
    
    LOG_INF("Connecting to WiFi SSID: %s", wifi_ssid);
    
    /* Setup connection parameters */
    wifi_params.ssid = wifi_ssid;
    wifi_params.ssid_length = strlen(wifi_ssid);
    wifi_params.psk = wifi_psk;
    wifi_params.psk_length = strlen(wifi_psk);
    wifi_params.channel = WIFI_CHANNEL_ANY;
    wifi_params.security = WIFI_SECURITY_TYPE_PSK;
    
    /* Initiate connection */
    ret = net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &wifi_params, sizeof(wifi_params));
    if (ret < 0) {
        LOG_ERR("WiFi connect request failed: %d", ret);
        return ret;
    }
    
    /* Wait for connection to be established */
    LOG_INF("WiFi connection requested");
    
    return 0;
}

/**
 * @brief Disconnect from WiFi
 *
 * @return 0 on success, negative errno on failure
 */
int wifi_disconnect(void)
{
    int ret;
    
    if (!is_connected) {
        return 0;
    }
    
    ret = net_mgmt(NET_REQUEST_WIFI_DISCONNECT, iface, NULL, 0);
    if (ret < 0) {
        LOG_ERR("WiFi disconnect request failed: %d", ret);
        return ret;
    }
    
    LOG_INF("WiFi disconnection requested");
    
    return 0;
}

/**
 * @brief Check if WiFi is connected
 *
 * @return true if connected, false otherwise
 */
bool wifi_is_connected(void)
{
    return is_connected;
}