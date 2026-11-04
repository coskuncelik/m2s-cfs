/*
 * os-impl-queues.c — m2s-cfs OSAL FreeRTOS layer: message queues with static storage.
 *
 * FreeRTOS queues have a fixed item size, OSAL messages have a variable size. Each item is
 * [size_t length][payload of max_size bytes]. Items are assembled in a per-queue staging
 * buffer (one for put, one for get, each protected by a static mutex), so exactly the
 * caller's `size` bytes are read on put and the real length is reported on get.
 *
 * Per-queue storage (OS_FREERTOS_QUEUE_STORAGE_BYTES) holds both staging buffers and
 * the queue itself: (depth + 2) * item_size must fit, otherwise OS_QUEUE_INVALID_SIZE.
 */
#include "os-freertos.h"
#include "os-shared-queue.h"
#include "os-shared-idmap.h"

typedef struct
{
    StaticQueue_t     queue_buf;
    QueueHandle_t     queue;
    StaticSemaphore_t put_mutex_buf;
    SemaphoreHandle_t put_mutex;
    StaticSemaphore_t get_mutex_buf;
    SemaphoreHandle_t get_mutex;
    size_t            item_size;
    uint8 *           put_stage;
    uint8 *           get_stage;
    union
    {
        uint8  bytes[OS_FREERTOS_QUEUE_STORAGE_BYTES];
        size_t align;
    } storage;
} OS_impl_queue_internal_record_t;

static OS_impl_queue_internal_record_t OS_impl_queue_table[OS_MAX_QUEUES];

int32 OS_FreeRTOS_QueueAPI_Impl_Init(void)
{
    memset(OS_impl_queue_table, 0, sizeof(OS_impl_queue_table));
    return OS_SUCCESS;
}

int32 OS_QueueCreate_Impl(const OS_object_token_t *token, uint32 flags)
{
    OS_impl_queue_internal_record_t *impl  = OS_OBJECT_TABLE_GET(OS_impl_queue_table, *token);
    OS_queue_internal_record_t *     queue = OS_OBJECT_TABLE_GET(OS_queue_table, *token);
    size_t                           item_size;

    (void)flags;

    item_size = sizeof(size_t) + queue->max_size;
    item_size = (item_size + sizeof(size_t) - 1) & ~(sizeof(size_t) - 1);

    if (queue->max_depth == 0 || item_size > sizeof(impl->storage.bytes) / ((size_t)queue->max_depth + 2))
    {
        OS_DEBUG("OS_QueueCreate_Impl: %lu x %lu bytes does not fit the static queue storage\n",
                 (unsigned long)queue->max_depth, (unsigned long)queue->max_size);
        return OS_QUEUE_INVALID_SIZE;
    }

    impl->item_size = item_size;
    impl->put_stage = &impl->storage.bytes[0];
    impl->get_stage = &impl->storage.bytes[item_size];
    impl->put_mutex = xSemaphoreCreateMutexStatic(&impl->put_mutex_buf);
    impl->get_mutex = xSemaphoreCreateMutexStatic(&impl->get_mutex_buf);
    impl->queue     = xQueueCreateStatic((UBaseType_t)queue->max_depth, (UBaseType_t)item_size,
                                     &impl->storage.bytes[2 * item_size], &impl->queue_buf);

    return (impl->queue != NULL && impl->put_mutex != NULL && impl->get_mutex != NULL) ? OS_SUCCESS : OS_ERROR;
}

int32 OS_QueueDelete_Impl(const OS_object_token_t *token)
{
    OS_impl_queue_internal_record_t *impl = OS_OBJECT_TABLE_GET(OS_impl_queue_table, *token);

    vQueueDelete(impl->queue);
    vSemaphoreDelete(impl->put_mutex);
    vSemaphoreDelete(impl->get_mutex);
    impl->queue = NULL;
    return OS_SUCCESS;
}

int32 OS_QueueGet_Impl(const OS_object_token_t *token, void *data, size_t size, size_t *size_copied, int32 timeout)
{
    OS_impl_queue_internal_record_t *impl = OS_OBJECT_TABLE_GET(OS_impl_queue_table, *token);
    TimeOut_t                        start;
    TickType_t                       ticks;
    size_t                           length;
    int32                            return_code;

    if (timeout == OS_PEND)
    {
        ticks = portMAX_DELAY;
    }
    else if (timeout == OS_CHECK)
    {
        ticks = 0;
    }
    else if (timeout < 0)
    {
        return OS_ERROR;
    }
    else
    {
        ticks = OS_FreeRTOS_MsToTicks((uint32)timeout);
    }

    vTaskSetTimeOutState(&start);
    if (xSemaphoreTake(impl->get_mutex, ticks) != pdTRUE)
    {
        return (timeout == OS_CHECK) ? OS_QUEUE_EMPTY : OS_QUEUE_TIMEOUT;
    }
    if (ticks != 0 && ticks != portMAX_DELAY)
    {
        (void)xTaskCheckForTimeOut(&start, &ticks); /* time left after waiting for the mutex */
    }

    if (xQueueReceive(impl->queue, impl->get_stage, ticks) == pdTRUE)
    {
        memcpy(&length, impl->get_stage, sizeof(length));
        if (length > size)
        {
            return_code = OS_QUEUE_INVALID_SIZE; /* cannot happen: shared layer checks size >= max_size */
        }
        else
        {
            memcpy(data, impl->get_stage + sizeof(size_t), length);
            *size_copied = length;
            return_code  = OS_SUCCESS;
        }
    }
    else if (timeout == OS_CHECK)
    {
        return_code = OS_QUEUE_EMPTY;
    }
    else if (timeout == OS_PEND)
    {
        return_code = OS_ERROR;
    }
    else
    {
        return_code = OS_QUEUE_TIMEOUT;
    }

    (void)xSemaphoreGive(impl->get_mutex);
    return return_code;
}

int32 OS_QueuePut_Impl(const OS_object_token_t *token, const void *data, size_t size, uint32 flags)
{
    OS_impl_queue_internal_record_t *impl = OS_OBJECT_TABLE_GET(OS_impl_queue_table, *token);
    int32                            return_code;

    (void)flags;

    if (xSemaphoreTake(impl->put_mutex, portMAX_DELAY) != pdTRUE)
    {
        return OS_ERROR;
    }

    memcpy(impl->put_stage, &size, sizeof(size));
    memcpy(impl->put_stage + sizeof(size_t), data, size);
    return_code = (xQueueSend(impl->queue, impl->put_stage, 0) == pdTRUE) ? OS_SUCCESS : OS_QUEUE_FULL;

    (void)xSemaphoreGive(impl->put_mutex);
    return return_code;
}

int32 OS_QueueGetInfo_Impl(const OS_object_token_t *token, OS_queue_prop_t *queue_prop)
{
    (void)token;
    (void)queue_prop;
    return OS_SUCCESS; /* the shared layer fills in name and creator */
}
