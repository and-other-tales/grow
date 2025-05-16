#include <zephyr/kernel.h>
#include <zephyr/net/http/client.h>
#include <zephyr/net/socket.h>
#include <zephyr/logging/log.h>
#include <zephyr/data/json.h>
#include <string.h>
#include <stdio.h>

#include "habitat_data.h"
#include "../storage.h"
#include "../connectivity.h"

LOG_MODULE_REGISTER(habitat_data, CONFIG_LOG_DEFAULT_LEVEL);

/* HTTP buffer sizes */
#define HTTP_BUF_SIZE 2048
#define HTTP_HEADER_SIZE 512

/* Static buffers */
static uint8_t http_rx_buf[HTTP_BUF_SIZE];
static uint8_t http_header_buf[HTTP_HEADER_SIZE];

/* HTTP client context */
static struct http_client_request http_req;
static struct http_client_response http_resp;
static struct k_sem http_sem;

/* Cache keys */
#define HABITAT_CACHE_KEY_PREFIX "habitat/"
#define HABITAT_CACHE_KEY_MAX 128

/* JSON parsing */
static int json_parse_handler(const char *key, size_t key_len,
                             const char *val, size_t val_len, void *data)
{
    struct habitat_data *habitat = (struct habitat_data *)data;
    
    /* Check field name and parse value accordingly */
    if (strncmp(key, "plantId", key_len) == 0) {
        if (val_len < sizeof(habitat->plant_id)) {
            memcpy(habitat->plant_id, val, val_len);
            habitat->plant_id[val_len] = '\0';
        }
    } else if (strncmp(key, "temperatureMinC", key_len) == 0) {
        char temp[32];
        memcpy(temp, val, val_len < 31 ? val_len : 31);
        temp[val_len < 31 ? val_len : 31] = '\0';
        habitat->ideal_temperature_min = strtof(temp, NULL);
    } else if (strncmp(key, "temperatureMaxC", key_len) == 0) {
        char temp[32];
        memcpy(temp, val, val_len < 31 ? val_len : 31);
        temp[val_len < 31 ? val_len : 31] = '\0';
        habitat->ideal_temperature_max = strtof(temp, NULL);
    } else if (strncmp(key, "humidityMin", key_len) == 0) {
        char temp[32];
        memcpy(temp, val, val_len < 31 ? val_len : 31);
        temp[val_len < 31 ? val_len : 31] = '\0';
        habitat->ideal_humidity_min = strtof(temp, NULL);
    } else if (strncmp(key, "humidityMax", key_len) == 0) {
        char temp[32];
        memcpy(temp, val, val_len < 31 ? val_len : 31);
        temp[val_len < 31 ? val_len : 31] = '\0';
        habitat->ideal_humidity_max = strtof(temp, NULL);
    } else if (strncmp(key, "soilMoistureMin", key_len) == 0) {
        char temp[32];
        memcpy(temp, val, val_len < 31 ? val_len : 31);
        temp[val_len < 31 ? val_len : 31] = '\0';
        habitat->ideal_soil_moisture_min = strtof(temp, NULL);
    } else if (strncmp(key, "soilMoistureMax", key_len) == 0) {
        char temp[32];
        memcpy(temp, val, val_len < 31 ? val_len : 31);
        temp[val_len < 31 ? val_len : 31] = '\0';
        habitat->ideal_soil_moisture_max = strtof(temp, NULL);
    } else if (strncmp(key, "lightLevelMin", key_len) == 0) {
        char temp[32];
        memcpy(temp, val, val_len < 31 ? val_len : 31);
        temp[val_len < 31 ? val_len : 31] = '\0';
        habitat->ideal_light_level_min = strtof(temp, NULL);
    } else if (strncmp(key, "lightLevelMax", key_len) == 0) {
        char temp[32];
        memcpy(temp, val, val_len < 31 ? val_len : 31);
        temp[val_len < 31 ? val_len : 31] = '\0';
        habitat->ideal_light_level_max = strtof(temp, NULL);
    } else if (strncmp(key, "nativeRegion", key_len) == 0) {
        if (val_len < sizeof(habitat->native_region)) {
            memcpy(habitat->native_region, val, val_len);
            habitat->native_region[val_len] = '\0';
        }
    } else if (strncmp(key, "growingSeason", key_len) == 0) {
        if (val_len < sizeof(habitat->growing_season)) {
            memcpy(habitat->growing_season, val, val_len);
            habitat->growing_season[val_len] = '\0';
        }
    }
    
    return 0;
}

/**
 * @brief HTTP response callback
 */
static void http_response_cb(struct http_response *rsp, enum http_final_call final_data, void *user_data)
{
    if (final_data == HTTP_DATA_FINAL) {
        k_sem_give(&http_sem);
    }
}

/**
 * @brief Initialize habitat data module
 * 
 * @return 0 on success, negative errno on failure
 */
int habitat_data_init(void)
{
    k_sem_init(&http_sem, 0, 1);
    return 0;
}

/**
 * @brief Generate cache key for habitat data
 */
static void generate_cache_key(const char *plant_name, const char *plant_variety, char *key_out, size_t key_size)
{
    snprintf(key_out, key_size, "%s%s_%s", HABITAT_CACHE_KEY_PREFIX, plant_name, plant_variety);
}

/**
 * @brief Fetch habitat data for a plant
 * 
 * @param plant_name Name of the plant
 * @param plant_variety Variety of the plant
 * @param data_out Pointer to store habitat data
 * @return 0 on success, negative errno on failure
 */
int habitat_data_fetch(const char *plant_name, const char *plant_variety, 
                      struct habitat_data *data_out)
{
    if (!connectivity_is_connected()) {
        /* Try to load from cache instead */
        LOG_WRN("Network not connected, trying to load from cache");
        return habitat_data_load_cache(plant_name, plant_variety, data_out);
    }

    int ret, sock;
    struct zsock_addrinfo hints, *addr;
    char host[] = "grow.othertales.co";
    char port[] = "443";
    char url[128];
    
    /* Setup HTTP request */
    memset(&http_req, 0, sizeof(http_req));
    memset(&http_resp, 0, sizeof(http_resp));
    
    /* Create URL */
    snprintf(url, sizeof(url), "/api/habitat?name=%s&variety=%s", plant_name, plant_variety);
    
    /* Resolve hostname */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    
    ret = zsock_getaddrinfo(host, port, &hints, &addr);
    if (ret) {
        LOG_ERR("Failed to resolve '%s': %d", host, ret);
        return -EHOSTUNREACH;
    }
    
    /* Create socket */
    sock = zsock_socket(addr->ai_family, addr->ai_socktype, IPPROTO_TLS_1_2);
    if (sock < 0) {
        LOG_ERR("Failed to create socket: %d", errno);
        zsock_freeaddrinfo(addr);
        return -errno;
    }
    
    /* Connect to server */
    ret = zsock_connect(sock, addr->ai_addr, addr->ai_addrlen);
    zsock_freeaddrinfo(addr);
    if (ret < 0) {
        LOG_ERR("Failed to connect: %d", errno);
        zsock_close(sock);
        return -errno;
    }
    
    /* Prepare HTTP request */
    http_req.method = HTTP_GET;
    http_req.url = url;
    http_req.host = host;
    http_req.protocol = "HTTP/1.1";
    http_req.recv_buf = http_rx_buf;
    http_req.recv_buf_len = sizeof(http_rx_buf);
    
    /* Set up response */
    http_resp.body_start = 1; /* Skip HTTP header */
    http_resp.body_buf = http_rx_buf;
    http_resp.body_buf_len = sizeof(http_rx_buf);
    http_resp.cb = http_response_cb;
    http_resp.recv_buf = http_rx_buf;
    http_resp.recv_buf_len = sizeof(http_rx_buf);
    http_resp.header_buf = http_header_buf;
    http_resp.header_buf_len = sizeof(http_header_buf);
    
    /* Send request */
    ret = http_client_req(sock, &http_req, &http_resp, 10000);
    if (ret < 0) {
        LOG_ERR("Failed to send HTTP request: %d", ret);
        zsock_close(sock);
        return ret;
    }
    
    /* Wait for response */
    if (k_sem_take(&http_sem, K_SECONDS(10)) != 0) {
        LOG_ERR("Timeout waiting for HTTP response");
        zsock_close(sock);
        return -ETIMEDOUT;
    }
    
    /* Close socket */
    zsock_close(sock);
    
    /* Check response status */
    if (http_resp.http_status_code != 200) {
        LOG_ERR("HTTP error: %d", http_resp.http_status_code);
        return -EINVAL;
    }
    
    /* Parse JSON response */
    struct json_obj_descr habitat_descr[11];
    json_obj_parse((char *)http_rx_buf, http_resp.body_frag_len, 
                  habitat_descr, ARRAY_SIZE(habitat_descr), 
                  json_parse_handler, data_out);
    
    /* Set data validity and timestamp */
    data_out->data_valid = true;
    data_out->timestamp = k_uptime_get() / 1000;
    
    /* Cache the data */
    ret = habitat_data_cache(data_out);
    if (ret < 0) {
        LOG_WRN("Failed to cache habitat data: %d", ret);
        /* Continue anyway since we have the data */
    }
    
    return 0;
}

/**
 * @brief Cache habitat data to storage
 * 
 * @param data Habitat data to cache
 * @return 0 on success, negative errno on failure
 */
int habitat_data_cache(const struct habitat_data *data)
{
    if (!data || !data->data_valid) {
        return -EINVAL;
    }
    
    char key[HABITAT_CACHE_KEY_MAX];
    generate_cache_key(data->plant_id, data->native_region, key, sizeof(key));
    
    int ret = storage_save_value(key, data, sizeof(struct habitat_data));
    if (ret < 0) {
        LOG_ERR("Failed to save habitat data to cache: %d", ret);
        return ret;
    }
    
    return 0;
}

/**
 * @brief Load habitat data from cache
 * 
 * @param plant_name Name of the plant
 * @param plant_variety Variety of the plant
 * @param data_out Pointer to store habitat data
 * @return 0 on success, negative errno on failure
 */
int habitat_data_load_cache(const char *plant_name, const char *plant_variety,
                           struct habitat_data *data_out)
{
    char key[HABITAT_CACHE_KEY_MAX];
    generate_cache_key(plant_name, plant_variety, key, sizeof(key));
    
    size_t data_size = sizeof(struct habitat_data);
    int ret = storage_load_value(key, data_out, &data_size);
    if (ret < 0) {
        LOG_ERR("Failed to load habitat data from cache: %d", ret);
        data_out->data_valid = false;
        return ret;
    }
    
    /* Check if data is still valid (1 day cache timeout) */
    int64_t now = k_uptime_get() / 1000;
    if (now - data_out->timestamp > 86400) {
        LOG_WRN("Cached habitat data is stale");
        data_out->data_valid = false;
        return -ESTALE;
    }
    
    return 0;
}