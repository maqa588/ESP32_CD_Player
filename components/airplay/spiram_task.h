#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/idf_additions.h"
#include "esp_heap_caps.h"

typedef struct {
  void *stack;
  void *tcb;
} spiram_task_mem_t;

static inline BaseType_t task_create_spiram(TaskFunction_t fn, const char *name,
                                            uint32_t depth, void *param,
                                            UBaseType_t prio,
                                            TaskHandle_t *handle,
                                            spiram_task_mem_t *mem) {
  if (mem) {
    mem->stack = NULL;
    mem->tcb = NULL;
  }
  // Allocate task stack and TCB from 8MB PSRAM first
  BaseType_t ret = xTaskCreatePinnedToCoreWithCaps(fn, name, depth, param, prio, handle, tskNO_AFFINITY, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (ret != pdPASS) {
    ret = xTaskCreate(fn, name, depth, param, prio, handle);
  }
  return ret;
}

static inline BaseType_t
task_create_pinned_spiram(TaskFunction_t fn, const char *name, uint32_t depth,
                          void *param, UBaseType_t prio, TaskHandle_t *handle,
                          BaseType_t core, spiram_task_mem_t *mem) {
  if (mem) {
    mem->stack = NULL;
    mem->tcb = NULL;
  }
  // Allocate task stack and TCB from 8MB PSRAM first
  BaseType_t ret = xTaskCreatePinnedToCoreWithCaps(fn, name, depth, param, prio, handle, core, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (ret != pdPASS) {
    ret = xTaskCreatePinnedToCore(fn, name, depth, param, prio, handle, core);
  }
  return ret;
}

static inline void task_free_spiram(spiram_task_mem_t *mem) {
  (void)mem;
}
