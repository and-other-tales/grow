#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/led.h>

#include "button_handler.h"

LOG_MODULE_REGISTER(button_handler, CONFIG_LOG_DEFAULT_LEVEL);

/* Button configuration */
#define BTN1_NODE DT_ALIAS(sw0) /* Use your board's button definitions */
static const struct gpio_dt_spec btn1 = GPIO_DT_SPEC_GET(BTN1_NODE, gpios);

/* LED configuration */
#define LED1_NODE DT_ALIAS(led0) /* Use your board's LED definitions */
static const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET(LED1_NODE, gpios);

/* Button press types */
#define BUTTON_PRESS_SHORT 1    /* Quick press */
#define BUTTON_PRESS_LONG 2     /* Long press (5+ seconds) */

/* Button state tracking */
static int btn1_pressed_time = 0;
static int btn1_released_time = 0;
static int btn1_press_count = 0;
static int btn1_press_type = 0;

/* Button request flags */
static bool reset_requested = false;
static bool factory_reset_requested = false;

/* Work items for button handling */
static struct k_work_delayable button_work;
static struct k_work_delayable led_work;

/* Timer for tracking button press duration */
static struct k_timer button_timer;

/* Current LED state for blinking */
static bool led_state = false;
static int blink_count = 0;

/**
 * @brief Button interrupt callback
 */
static void button_cb(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    int val = gpio_pin_get_dt(&btn1);
    
    if (val) {
        /* Button pressed */
        btn1_pressed_time = k_uptime_get_32();
        
        /* Start timer for long press detection */
        k_timer_start(&button_timer, K_SECONDS(5), K_NO_WAIT);
    } else {
        /* Button released */
        btn1_released_time = k_uptime_get_32();
        int press_duration = btn1_released_time - btn1_pressed_time;
        
        /* Stop timer */
        k_timer_stop(&button_timer);
        
        if (press_duration < 1000) {
            /* Short press */
            btn1_press_count++;
            btn1_press_type = BUTTON_PRESS_SHORT;
            
            /* Schedule work to process multiple presses */
            k_work_schedule(&button_work, K_MSEC(500));
        }
        /* Long press is handled by timer */
    }
}

/**
 * @brief Button timer expiry (long press detected)
 */
static void button_timer_expiry(struct k_timer *timer)
{
    /* Long press detected */
    btn1_press_type = BUTTON_PRESS_LONG;
    
    /* Schedule work immediately */
    k_work_schedule(&button_work, K_NO_WAIT);
    
    /* Start LED blinking for visual feedback */
    blink_count = 0;
    k_work_schedule(&led_work, K_NO_WAIT);
}

/**
 * @brief LED work handler for blink pattern
 */
static void led_work_handler(struct k_work *work)
{
    /* Toggle LED */
    led_state = !led_state;
    gpio_pin_set_dt(&led1, led_state);
    
    /* Continue blinking if needed */
    if (blink_count < 4) { /* 2 full blinks (on-off-on-off) */
        blink_count++;
        k_work_schedule(&led_work, K_MSEC(250));
    } else {
        /* Turn off LED when done */
        gpio_pin_set_dt(&led1, 0);
    }
}

/**
 * @brief Button work handler
 */
static void button_work_handler(struct k_work *work)
{
    if (btn1_press_type == BUTTON_PRESS_SHORT) {
        if (btn1_press_count == 2) {
            /* Double press - request soft reset */
            LOG_INF("Double press detected - requesting soft reset");
            reset_requested = true;
        }
        
        /* Reset press count */
        btn1_press_count = 0;
    } else if (btn1_press_type == BUTTON_PRESS_LONG) {
        /* Long press - request factory reset */
        LOG_INF("Long press detected - requesting factory reset");
        factory_reset_requested = true;
    }
    
    /* Reset press type */
    btn1_press_type = 0;
}

/**
 * @brief Initialize button handler
 * 
 * @return 0 on success, negative errno on failure
 */
int button_handler_init(void)
{
    int ret;
    static struct gpio_callback btn_cb;
    
    /* Configure button */
    if (!device_is_ready(btn1.port)) {
        LOG_ERR("Button device not ready");
        return -ENODEV;
    }
    
    ret = gpio_pin_configure_dt(&btn1, GPIO_INPUT);
    if (ret < 0) {
        LOG_ERR("Failed to configure button: %d", ret);
        return ret;
    }
    
    ret = gpio_pin_interrupt_configure_dt(&btn1, GPIO_INT_EDGE_BOTH);
    if (ret < 0) {
        LOG_ERR("Failed to configure button interrupt: %d", ret);
        return ret;
    }
    
    gpio_init_callback(&btn_cb, button_cb, BIT(btn1.pin));
    gpio_add_callback(btn1.port, &btn_cb);
    
    /* Configure LED */
    if (!device_is_ready(led1.port)) {
        LOG_ERR("LED device not ready");
        return -ENODEV;
    }
    
    ret = gpio_pin_configure_dt(&led1, GPIO_OUTPUT);
    if (ret < 0) {
        LOG_ERR("Failed to configure LED: %d", ret);
        return ret;
    }
    
    /* Initialize work items */
    k_work_init_delayable(&button_work, button_work_handler);
    k_work_init_delayable(&led_work, led_work_handler);
    
    /* Initialize button timer */
    k_timer_init(&button_timer, button_timer_expiry, NULL);
    
    LOG_INF("Button handler initialized");
    return 0;
}

/**
 * @brief Check if a reset was requested via button
 * 
 * @return true if reset requested, false otherwise
 */
bool button_reset_requested(void)
{
    return reset_requested;
}

/**
 * @brief Check if a factory reset was requested via button
 * 
 * @return true if factory reset requested, false otherwise
 */
bool button_factory_reset_requested(void)
{
    return factory_reset_requested;
}

/**
 * @brief Clear button request flags
 */
void button_clear_requests(void)
{
    reset_requested = false;
    factory_reset_requested = false;
}