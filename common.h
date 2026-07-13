#ifndef COMMON_H
#define COMMON_H

#include <windows.h>
#include "config.h"

typedef struct {
    char   sensorId[SENSOR_ID_LEN];
    char   timestamp[TIMESTAMP_LEN];
    int    eventType;
    double value;
} Event;

typedef struct {
    volatile LONG head;
    volatile LONG tail;
    volatile LONG count;
    long          maxSize;
    Event         events[BUFFER_SIZE];
} CircularBuffer;

typedef struct {
    long          bufferOffset;
    long          bufferMaxSize;
    volatile LONG activeSensors;
    volatile LONG running;
} SharedHeader;

#define SHM_SIZE (sizeof(SharedHeader) + sizeof(CircularBuffer))

#endif
