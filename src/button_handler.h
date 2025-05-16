#ifndef BUTTON_HANDLER_H
#define BUTTON_HANDLER_H

/**
 * @brief Initialize button handler
 * 
 * @return 0 on success, negative errno on failure
 */
int button_handler_init(void);

/**
 * @brief Check if a reset was requested via button
 * 
 * @return true if reset requested, false otherwise
 */
bool button_reset_requested(void);

/**
 * @brief Check if a factory reset was requested via button
 * 
 * @return true if factory reset requested, false otherwise
 */
bool button_factory_reset_requested(void);

/**
 * @brief Clear button request flags
 */
void button_clear_requests(void);

#endif /* BUTTON_HANDLER_H */