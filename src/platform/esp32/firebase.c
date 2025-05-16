#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/http/client.h>
#include <zephyr/net/dns_resolve.h>
#include <zephyr/data/json.h>
#include <string.h>
#include <stdio.h>

#include "../../firebase.h"

LOG_MODULE_REGISTER(firebase, CONFIG_LOG_DEFAULT_LEVEL);

/* Firebase configuration */
#define FIREBASE_HOST "firestore.googleapis.com"
#define FIREBASE_PORT 443
#define FIREBASE_API_VERSION "v1"

/* Your Firebase project details */
#define FIREBASE_PROJECT_ID "growsense-12345" /* Replace with your project ID */

/* HTTP buffer sizes */
#define MAX_PAYLOAD_SIZE 1024
#define MAX_RESPONSE_SIZE 512
#define MAX_HEADER_SIZE 256

/* Static buffers */
static uint8_t payload_buf[MAX_PAYLOAD_SIZE];
static uint8_t response_buf[MAX_RESPONSE_SIZE];
static uint8_t header_buf[MAX_HEADER_SIZE];

/* HTTP client configuration */
static struct http_client_request req;
static struct http_client_response rsp;

/* Socket */
static int sock;

/**
 * @brief Initialize Firebase connection
 *
 * @return 0 on success, negative errno on failure
 */
int firebase_init(void)
{
    LOG_INF("Initializing Firebase connection");
    
    /* Firebase initialization is basically establishing HTTP comm */
    /* Real initialization will happen during first data transmission */
    
    return 0;
}

/**
 * @brief Create JSON payload for sensor data
 *
 * @param payload Buffer to store payload
 * @param payload_size Size of payload buffer
 * @param soil_moisture Soil moisture value
 * @param light_level Light level value
 * @param temperature Temperature value
 * @param humidity Humidity value
 * @param air_movement Air movement value
 * @param timestamp Timestamp of reading
 * @param plant_name Plant name
 * @param plant_variety Plant variety
 * @param health_status Plant health status
 * @param env_mismatch Environmental mismatch flags
 * @param recommendation Recommendations
 * @param plant_status Plant status string
 * @return Length of payload on success, negative errno on failure
 */
static int create_sensor_data_payload(char *payload, size_t payload_size,
                                    float soil_moisture,
                                    float light_level,
                                    float temperature,
                                    float humidity,
                                    float air_movement,
                                    int64_t timestamp,
                                    const char *plant_name,
                                    const char *plant_variety,
                                    int health_status,
                                    const char *env_mismatch,
                                    const char *recommendation,
                                    const char *plant_status)
{
    /* Create JSON payload according to Firestore API format */
    int len = snprintf(payload, payload_size,
                     "{"
                     "\"fields\": {"
                     "\"soilMoisture\": {\"doubleValue\": %.2f},"
                     "\"lightLevel\": {\"doubleValue\": %.2f},"
                     "\"temperature\": {\"doubleValue\": %.2f},"
                     "\"humidity\": {\"doubleValue\": %.2f},"
                     "\"airMovement\": {\"doubleValue\": %.2f},"
                     "\"timestamp\": {\"integerValue\": \"%lld\"},"
                     "\"plantName\": {\"stringValue\": \"%s\"},"
                     "\"plantVariety\": {\"stringValue\": \"%s\"},"
                     "\"healthStatus\": {\"integerValue\": \"%d\"},"
                     "\"environmentalMismatch\": {\"stringValue\": \"%s\"},"
                     "\"recommendation\": {\"stringValue\": \"%s\"},"
                     "\"plantStatus\": {\"stringValue\": \"%s\"}"
                     "}"
                     "}",
                     soil_moisture, light_level, temperature, humidity,
                     air_movement, (long long)timestamp, plant_name, plant_variety,
                     health_status, env_mismatch, recommendation, plant_status);
                     
    if (len < 0 || len >= payload_size) {
        LOG_ERR("Payload buffer too small");
        return -ENOMEM;
    }
    
    return len;
}

/**
 * @brief Send sensor data to Firebase
 *
 * @param serial_number Device serial number
 * @param soil_moisture Soil moisture value (0-100%)
 * @param light_level Light level value (0-100%)
 * @param temperature Temperature value (°C)
 * @param humidity Humidity value (0-100%)
 * @param air_movement Air movement value
 * @param timestamp Timestamp of reading
 * @param plant_name Plant name
 * @param plant_variety Plant variety
 * @param health_status Plant health status
 * @param env_mismatch Environmental mismatch flags
 * @param recommendation Recommendations
 * @param plant_status Plant status string
 * @return 0 on success, negative errno on failure
 */
int firebase_send_sensor_data(const char *serial_number,
                             float soil_moisture,
                             float light_level,
                             float temperature,
                             float humidity,
                             float air_movement,
                             int64_t timestamp,
                             const char *plant_name,
                             const char *plant_variety,
                             int health_status,
                             const char *env_mismatch,
                             const char *recommendation,
                             const char *plant_status)
{
    int ret;
    struct sockaddr_in addr;
    struct zsock_addrinfo *addrinfo, hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM
    };
    int payload_len;
    char url[128];
    
    LOG_INF("Sending sensor data to Firebase");
    
    /* Resolve Firebase host */
    ret = zsock_getaddrinfo(FIREBASE_HOST, NULL, &hints, &addrinfo);
    if (ret < 0) {
        LOG_ERR("Failed to resolve Firebase host: %d", ret);
        return ret;
    }
    
    /* Create socket */
    sock = zsock_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) {
        LOG_ERR("Failed to create socket: %d", errno);
        zsock_freeaddrinfo(addrinfo);
        return -errno;
    }
    
    /* Setup address */
    memcpy(&addr, addrinfo->ai_addr, sizeof(addr));
    addr.sin_port = htons(FIREBASE_PORT);
    
    zsock_freeaddrinfo(addrinfo);
    
    /* Connect to Firebase */
    ret = zsock_connect(sock, (struct sockaddr *)&addr, sizeof(addr));
    if (ret < 0) {
        LOG_ERR("Failed to connect to Firebase: %d", errno);
        zsock_close(sock);
        return -errno;
    }
    
    /* Create URL for the document */
    snprintf(url, sizeof(url),
            "/v1/projects/%s/databases/(default)/documents/plants/%s",
            FIREBASE_PROJECT_ID, serial_number);
    
    /* Create JSON payload for sensor data */
    payload_len = create_sensor_data_payload((char *)payload_buf, sizeof(payload_buf),
                                           soil_moisture, light_level,
                                           temperature, humidity, air_movement,
                                           timestamp, plant_name, plant_variety,
                                           health_status, env_mismatch, recommendation, 
                                           plant_status);
    if (payload_len < 0) {
        zsock_close(sock);
        return payload_len;
    }
    
    /* Setup HTTP request */
    memset(&req, 0, sizeof(req));
    memset(&rsp, 0, sizeof(rsp));
    
    req.method = HTTP_PATCH;
    req.url = url;
    req.host = FIREBASE_HOST;
    req.protocol = "https";
    req.payload = payload_buf;
    req.payload_len = payload_len;
    req.content_type_value = "application/json";
    
    rsp.data = response_buf;
    rsp.data_len = sizeof(response_buf);
    rsp.header_buf = header_buf;
    rsp.header_buf_len = sizeof(header_buf);
    
    /* Send HTTP request */
    ret = http_client_req(sock, &req, 5000, &rsp);
    
    zsock_close(sock);
    
    if (ret < 0) {
        LOG_ERR("Failed to send HTTP request: %d", ret);
        return ret;
    }
    
    if (rsp.status_code != 200 && rsp.status_code != 201) {
        LOG_ERR("Firebase request failed with status %d", rsp.status_code);
        LOG_ERR("Response: %s", rsp.data);
        return -EIO;
    }
    
    LOG_INF("Sensor data sent to Firebase successfully");
    
    return 0;
}

/**
 * @brief Send water prediction data to Firebase
 *
 * @param serial_number Device serial number
 * @param daily_consumption_rate Daily water consumption rate
 * @param next_watering_timestamp Predicted next watering timestamp
 * @param prediction_confidence Confidence level in prediction
 * @return 0 on success, negative errno on failure
 */
int firebase_send_water_prediction(const char *serial_number,
                                 float daily_consumption_rate,
                                 int64_t next_watering_timestamp,
                                 float prediction_confidence)
{
    int ret;
    struct sockaddr_in addr;
    struct zsock_addrinfo *addrinfo, hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM
    };
    char payload[256];
    char url[128];
    
    LOG_INF("Sending water prediction to Firebase");
    
    /* Create payload */
    snprintf(payload, sizeof(payload),
            "{"
            "\"fields\": {"
            "\"dailyConsumptionRate\": {\"doubleValue\": %.2f},"
            "\"nextWateringTime\": {\"integerValue\": \"%lld\"},"
            "\"predictionConfidence\": {\"doubleValue\": %.2f}"
            "}"
            "}",
            daily_consumption_rate, 
            (long long)next_watering_timestamp,
            prediction_confidence);
    
    /* Create URL for the document */
    snprintf(url, sizeof(url),
            "/v1/projects/%s/databases/(default)/documents/plants/%s/waterPrediction/current",
            FIREBASE_PROJECT_ID, serial_number);
    
    /* Resolve Firebase host */
    ret = zsock_getaddrinfo(FIREBASE_HOST, NULL, &hints, &addrinfo);
    if (ret < 0) {
        LOG_ERR("Failed to resolve Firebase host: %d", ret);
        return ret;
    }
    
    /* Create socket */
    sock = zsock_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) {
        LOG_ERR("Failed to create socket: %d", errno);
        zsock_freeaddrinfo(addrinfo);
        return -errno;
    }
    
    /* Setup address */
    memcpy(&addr, addrinfo->ai_addr, sizeof(addr));
    addr.sin_port = htons(FIREBASE_PORT);
    
    zsock_freeaddrinfo(addrinfo);
    
    /* Connect to Firebase */
    ret = zsock_connect(sock, (struct sockaddr *)&addr, sizeof(addr));
    if (ret < 0) {
        LOG_ERR("Failed to connect to Firebase: %d", errno);
        zsock_close(sock);
        return -errno;
    }
    
    /* Setup HTTP request */
    memset(&req, 0, sizeof(req));
    memset(&rsp, 0, sizeof(rsp));
    
    req.method = HTTP_PATCH;
    req.url = url;
    req.host = FIREBASE_HOST;
    req.protocol = "https";
    req.payload = payload;
    req.payload_len = strlen(payload);
    req.content_type_value = "application/json";
    
    rsp.data = response_buf;
    rsp.data_len = sizeof(response_buf);
    rsp.header_buf = header_buf;
    rsp.header_buf_len = sizeof(header_buf);
    
    /* Send HTTP request */
    ret = http_client_req(sock, &req, 5000, &rsp);
    
    zsock_close(sock);
    
    if (ret < 0) {
        LOG_ERR("Failed to send HTTP request: %d", ret);
        return ret;
    }
    
    if (rsp.status_code != 200 && rsp.status_code != 201) {
        LOG_ERR("Firebase request failed with status %d", rsp.status_code);
        LOG_ERR("Response: %s", rsp.data);
        return -EIO;
    }
    
    LOG_INF("Water prediction data sent to Firebase successfully");
    
    return 0;
}