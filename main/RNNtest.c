#include <stdio.h>
#include "rnnoise.h"
#include "esp_timer.h"
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "esp_task_wdt.h"

#define FRAME_SIZE 480

// for tracing stack high water mark
// static UBaseType_t uxHighWaterMark;

//#define RNNOISE_TASK_PRIORITY (configMAX_PRIORITIES - 2)

typedef struct {
    DenoiseState *st;
    float *out;
    float *in;
    TaskHandle_t parent_task;
} RNNoiseParams;

// debugging print functions
void print_memory_usage() {
  size_t free_dram = heap_caps_get_free_size(MALLOC_CAP_INTERNAL)/1024;
  size_t free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM)/1024;
  size_t free_block = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)/1024;
  size_t total_free = free_dram + free_psram;

  printf("\nCurrent Memory Usage:\n");
  printf("Free DRAM: %d KB\n", free_dram);
  printf("Free PSRAM: %d KB\n", free_psram);
  printf("Largest free block: %d KB\n", free_block);
  printf("Total Free Memory: %d KB\n\n", total_free);
}

void print_max_ram() {
  multi_heap_info_t info;
  heap_caps_get_info(&info, MALLOC_CAP_INTERNAL);
  printf("\nRam high-water mark:\n");
  printf("Min free DRAM: %u KB\n", info.minimum_free_bytes/1024);
}

// Task to handle frame processing
static void process_frame_task(void *pvParameters) {
  // uxHighWaterMark = uxTaskGetStackHighWaterMark( NULL );
  
  RNNoiseParams *params = (RNNoiseParams*)pvParameters;

  // process frame
  rnnoise_process_frame(params->st, params->out, params->in);

  // uxHighWaterMark = uxTaskGetStackHighWaterMark( NULL );

  // Notify parent task that processing is complete
  xTaskNotifyGive(params->parent_task);

  heap_caps_free(params);

  // Task will delete itself
  vTaskDelete(NULL);
}

// create a task to call rnnoise_process_frame
// rnnnoise_process_frame requires ~45kB stack
void rnnoise_process_frame_task_wrapper(DenoiseState *st, float *out, float *in) {
  TaskHandle_t xHandle = NULL;

  RNNoiseParams *params = (RNNoiseParams*)heap_caps_malloc(sizeof(RNNoiseParams), MALLOC_CAP_INTERNAL);

  params->st = st;
  params->out = out;
  params->in = in;
  params->parent_task = xTaskGetCurrentTaskHandle();

  // create a task for rnnoise_process_frame
  BaseType_t task = xTaskCreate(
      process_frame_task,
      "rnnoise_task",
      45000,
      (void *)params,
      tskIDLE_PRIORITY + 1,
      &xHandle
  );

  // check that task was created
  if (task != pdPASS) {
    return;
  }

  // Wait for task completion or timeout
  if (xTaskNotifyWait(0, 0, NULL, pdMS_TO_TICKS(2000)) != pdTRUE) {
      printf("Frame processing timeout!\n");
      vTaskDelete(xHandle);
  }
}

void app_main(void)
{

  DenoiseState* st = rnnoise_create(NULL);

  float x[FRAME_SIZE];

  // run once without timing to init
  memset(x, 0x00, sizeof(x));
  rnnoise_process_frame_task_wrapper(st, x, x);

  const int benchmark_iterations = 50;
  uint64_t rnnoise_process_frame_time = 0;

  for (int j = 0; j < benchmark_iterations; j++) {
    for (int i = 0; i < FRAME_SIZE; i++) {
      x[i] = ((float)rand()) / ((float)RAND_MAX) * 0x7fff;
    }

    uint64_t usec_rnnoise_process_frame_start = esp_timer_get_time();

    rnnoise_process_frame_task_wrapper(st, x, x);

    uint64_t usec_rnnoise_process_frame_end = esp_timer_get_time();

    rnnoise_process_frame_time += (usec_rnnoise_process_frame_end - usec_rnnoise_process_frame_start);
  }

  printf("rnnoise_process_frame %llu usec\n", (rnnoise_process_frame_time / benchmark_iterations));
  printf("rnnoise_process_frame %llu msec\n", (rnnoise_process_frame_time / benchmark_iterations)/1000);

  //print_max_ram();

  // printf("Stack high water mark: %d\n", uxHighWaterMark);

}
