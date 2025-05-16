#ifndef TFLITE_INTERFACE_H
#define TFLITE_INTERFACE_H

#include <stdint.h>
#include <stddef.h>

/**
 * @brief TensorFlow Lite context structure
 */
struct tflite_context {
    void *model_data;       /* Pointer to model data in memory */
    void *interpreter;      /* TFLite interpreter instance */
    void *input_tensor;     /* Input tensor pointer */
    void *output_tensor;    /* Output tensor pointer */
    size_t arena_size;      /* Size of tensor arena */
    uint8_t *tensor_arena;  /* Memory area for tensor allocations */
};

/**
 * @brief Initialize TensorFlow Lite
 * 
 * Loads model and sets up TensorFlow Lite interpreter
 * 
 * @param ctx Pointer to TFLite context
 * @return 0 on success, negative errno on failure
 */
int tflite_init(struct tflite_context *ctx);

/**
 * @brief Run inference on input data
 * 
 * @param ctx TFLite context
 * @param input_data Input sensor data array
 * @param input_size Size of input data array
 * @param output_data Buffer to store inference results
 * @param output_size Size of output buffer
 * @return 0 on success, negative errno on failure
 */
int tflite_run_inference(struct tflite_context *ctx, 
                         const float *input_data, size_t input_size,
                         float *output_data, size_t output_size);

/**
 * @brief Clean up TensorFlow Lite resources
 * 
 * @param ctx TFLite context
 * @return 0 on success, negative errno on failure
 */
int tflite_deinit(struct tflite_context *ctx);

#endif /* TFLITE_INTERFACE_H */