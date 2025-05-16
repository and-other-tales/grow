#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/fs/fs.h>
#include <string.h>

#include "../../tflite_interface.h"

/* TensorFlow Lite Micro headers */
#ifdef __cplusplus
extern "C" {
#endif

#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_log.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/system_setup.h"
#include "tensorflow/lite/schema/schema_generated.h"

#ifdef __cplusplus
}
#endif

LOG_MODULE_REGISTER(tflite_esp, CONFIG_LOG_DEFAULT_LEVEL);

/* Static TF Lite objects */
namespace {
  const tflite::Model* model = nullptr;
  tflite::MicroInterpreter* interpreter = nullptr;
  tflite::MicroMutableOpResolver<10> op_resolver;

  /* Create an area of memory for input, output, and intermediate arrays */
  constexpr int kTensorArenaSize = 128 * 1024;
  static uint8_t tensor_arena[kTensorArenaSize];
} // namespace

/* Path to model file in flash */
#define MODEL_PATH "/tflite/plant_health_model.tflite"
#define MODEL_SIZE (32 * 1024) /* Maximum expected model size */

/* Buffer for model loading */
static uint8_t model_data[MODEL_SIZE];

/**
 * @brief Initialize TensorFlow Lite
 * 
 * @param ctx Pointer to TFLite context
 * @return 0 on success, negative errno on failure
 */
int tflite_init(struct tflite_context *ctx)
{
    if (!ctx) {
        return -EINVAL;
    }
    
    LOG_INF("Initializing TensorFlow Lite for ESP32");
    
    /* Setup TFLite micro */
    tflite::InitializeTarget();
    
    /* Load the model from flash */
    struct fs_file_t file;
    fs_file_t_init(&file);
    int rc = fs_open(&file, MODEL_PATH, FS_O_READ);
    if (rc != 0) {
        LOG_ERR("Failed to open model file: %d", rc);
        return -EIO;
    }
    
    /* Read model file */
    size_t bytes_read;
    rc = fs_read(&file, model_data, MODEL_SIZE, &bytes_read);
    fs_close(&file);
    
    if (rc != 0) {
        LOG_ERR("Failed to read model file: %d", rc);
        return -EIO;
    }
    
    LOG_INF("Model loaded, size: %d bytes", bytes_read);
    
    /* Map the model into a usable data structure */
    model = tflite::GetModel(model_data);
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        LOG_ERR("Model schema version mismatch, expected %d but got %d",
               TFLITE_SCHEMA_VERSION, model->version());
        return -EINVAL;
    }
    
    /* Add required operations to the resolver */
    op_resolver.AddFullyConnected();
    op_resolver.AddRelu();
    op_resolver.AddReshape();
    op_resolver.AddSoftmax();
    op_resolver.AddPad();
    op_resolver.AddMean();
    op_resolver.AddConv2D();
    op_resolver.AddMaxPool2D();
    op_resolver.AddQuantize();
    op_resolver.AddDequantize();
    
    /* Build an interpreter to run the model */
    interpreter = new tflite::MicroInterpreter(
        model, op_resolver, tensor_arena, kTensorArenaSize);
    
    /* Allocate tensors */
    if (interpreter->AllocateTensors() != kTfLiteOk) {
        LOG_ERR("Failed to allocate tensors");
        return -ENOMEM;
    }
    
    LOG_INF("Tensors allocated, arena used: %d bytes", 
           interpreter->arena_used_bytes());
    
    /* Set up context */
    ctx->model_data = (void*)model;
    ctx->interpreter = (void*)interpreter;
    ctx->tensor_arena = tensor_arena;
    ctx->arena_size = kTensorArenaSize;
    
    return 0;
}

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
                         float *output_data, size_t output_size)
{
    if (!ctx || !input_data || !output_data) {
        return -EINVAL;
    }
    
    tflite::MicroInterpreter *interpreter = (tflite::MicroInterpreter *)ctx->interpreter;
    
    /* Get input tensor */
    TfLiteTensor *input = interpreter->input(0);
    
    /* Check input dimensions */
    if (input->dims->size != 2 || input->dims->data[1] != input_size) {
        LOG_ERR("Unexpected input dimensions");
        return -EINVAL;
    }
    
    /* Copy input data */
    for (size_t i = 0; i < input_size; i++) {
        input->data.f[i] = input_data[i];
    }
    
    /* Run inference */
    if (interpreter->Invoke() != kTfLiteOk) {
        LOG_ERR("Inference failed");
        return -EFAULT;
    }
    
    /* Get output tensor */
    TfLiteTensor *output = interpreter->output(0);
    
    /* Check output dimensions */
    if (output->dims->size != 2 || output->dims->data[1] != output_size) {
        LOG_ERR("Unexpected output dimensions");
        return -EINVAL;
    }
    
    /* Copy output data */
    for (size_t i = 0; i < output_size; i++) {
        output_data[i] = output->data.f[i];
    }
    
    return 0;
}

/**
 * @brief Clean up TensorFlow Lite resources
 * 
 * @param ctx TFLite context
 * @return 0 on success, negative errno on failure
 */
int tflite_deinit(struct tflite_context *ctx)
{
    if (!ctx) {
        return -EINVAL;
    }
    
    if (ctx->interpreter) {
        delete static_cast<tflite::MicroInterpreter*>(ctx->interpreter);
        ctx->interpreter = nullptr;
    }
    
    /* No need to delete model as it points to model_data */
    ctx->model_data = nullptr;
    
    return 0;
}