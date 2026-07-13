#ifndef CONFIG_H
#define CONFIG_H

#define PIPE_NAME           "\\\\.\\pipe\\SensorPipe"
#define SHM_NAME            "Global\\SensorSharedMemory"
#define MUTEX_NAME          "Global\\CircularBufferMutex"
#define SEM_EMPTY_NAME      "Global\\SemEmpty"
#define SEM_FULL_NAME       "Global\\SemFull"

#define BUFFER_SIZE         10
#define MAX_CLIENTS         16
#define SENSOR_ID_LEN       32
#define TIMESTAMP_LEN       64

#endif
