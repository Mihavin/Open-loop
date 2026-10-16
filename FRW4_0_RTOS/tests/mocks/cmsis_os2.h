#ifndef MOCK_OS_H
#define MOCK_OS_H
#include <stdint.h>
#include <stddef.h>
typedef void* osMessageQueueId_t;
typedef enum { osOK=0, osError=-1 } osStatus_t;
osMessageQueueId_t osMessageQueueNew(uint32_t,uint32_t,const void*);
osStatus_t osMessageQueuePut(osMessageQueueId_t,const void*,uint8_t,uint32_t);
osStatus_t osMessageQueueGet(osMessageQueueId_t,void*,uint8_t*,uint32_t);
#endif
