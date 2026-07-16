/******************************************************************************
 * NOMBRE: Miguel Rivas
 * PROYECTO: Sistema de Telemetría y Control Concurrente (Fórmula 1) - MÓDULO 3
 * DESCRIPCIÓN: Consumidor del búfer circular en memoria compartida. Clasifica 
 *              los eventos por prioridad y los distribuye mediante un puerto 
 *              de finalización (IOCP) a un Pool de Workers para su persistencia 
 *              segura en un archivo log usando bloqueos de archivos (LockFileEx).
 * FECHA: 16 de julio de 2026
 *****************************************************************************/

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include "common.h"
#include "config.h"

// Constantes de diseño del módulo
#define NUM_WORKERS 4               // Número de hilos trabajadores en el Pool
#define LOG_FILE_NAME "telemetry.log" // Nombre del archivo de salida de datos
#define EXIT_CODE_FLAG 0xFFFFFFFF   // Valor especial enviado al IOCP para apagar los hilos

// Estructura para transferir el trabajo desde el Dispatcher hacia los Workers por IOCP
typedef struct {
    Event event;                    // Estructura original del evento de telemetría
    int priority;                   // Prioridad calculada del evento (1: Crítico, 2: Normal)
} WorkItem;

// Declaración global de Handles para que los Workers puedan acceder a ellos
HANDLE hLogFile = INVALID_HANDLE_VALUE; // Handle del archivo de persistencia compartida
HANDLE hIOCP = INVALID_HANDLE_VALUE;    // Handle del puerto de finalización (cola de tareas)

// Función que ejecutan los hilos trabajadores (Workers) del Pool
DWORD WINAPI WorkerThread(LPVOID lpParam) {
    DWORD dwBytesTransfered;        // Almacena los bytes transferidos (no se usa aquí, requerido por API)
    ULONG_PTR completionKey;        // Clave de completación del IOCP (no se usa aquí, requerido por API)
    LPOVERLAPPED lpOverlapped;      // Puntero que contendrá la estructura del trabajo enviado

    while (TRUE) {
        // Se duerme el hilo esperando que el IOCP le asigne un evento (Uso de CPU: 0%)
        BOOL success = GetQueuedCompletionStatus(
            hIOCP,                  // Puerto del que lee
            &dwBytesTransfered,     // Puntero para recibir tamaño de transferencia
            &completionKey,         // Recibe la clave asociada al puerto
            &lpOverlapped,          // Recibe la dirección de nuestra estructura de datos
            INFINITE                // Espera indefinida (sin encuestas activas)
        );

        // Si se extrajo un item del puerto exitosamente
        if (success && lpOverlapped != NULL) {
            // Conversión del puntero genérico a nuestra estructura de datos de trabajo
            WorkItem* work = (WorkItem*)lpOverlapped;

            // Protocolo de apagado limpio: si la prioridad es el código especial, termina el hilo
            if (work->priority == EXIT_CODE_FLAG) {
                free(work);         // Liberación de la memoria asignada para la señal de salida
                break;              // Rompe el bucle para finalizar la ejecución del hilo
            }

            // --- PERSISTENCIA CONCURRENTE SEGURA EN EL ARCHIVO DE LOG ---
            
            // Estructura para configurar el bloqueo asíncrono y la posición del archivo
            OVERLAPPED ovLock = { 0 };
            // Establecemos que escriba siempre al final del archivo de texto
            ovLock.Offset = 0xFFFFFFFF;       // Desplazamiento bajo (indica fin del archivo en modo append)
            ovLock.OffsetHigh = 0xFFFFFFFF;   // Desplazamiento alto

            // Bloqueamos la región de escritura en el archivo de forma exclusiva usando LockFileEx
            BOOL locked = LockFileEx(
                hLogFile,                     // Handle del archivo de log abierto de forma compartida
                LOCKFILE_EXCLUSIVE_LOCK,      // Tipo de bloqueo exclusivo (impide que otros hilos escriban)
                0,                            // Reservado (debe ser 0)
                1,                            // Longitud de bytes a bloquear (basta con 1 byte al final)
                0,                            // Longitud alta a bloquear
                &ovLock                       // Puntero a la estructura de posición
            );

            if (locked) {
                char logBuffer[256];          // Búfer temporal para formatear la cadena de texto
                
                // Formateamos los datos del evento junto con su nivel de prioridad calculado
                int len = snprintf(logBuffer, sizeof(logBuffer),
                    "[PRIO: %d] ID: %s | Time: %s | Type: %d | Val: %.2f\n",
                    work->priority, work->event.sensorId, work->event.timestamp,
                    work->event.eventType, work->event.value);

                DWORD bytesWritten;           // Variable para recibir el conteo de bytes escritos
                // Escritura real y física de la cadena de datos en el archivo
                WriteFile(hLogFile, logBuffer, (DWORD)len, &bytesWritten, &ovLock);

                // Liberamos el bloqueo de forma inmediata para que otros Workers puedan escribir
                UnlockFileEx(hLogFile, 0, 1, 0, &ovLock);
                
                // Mostramos en la consola del dispatcher el procesamiento en tiempo real
                printf("[Worker %u] Evento de %s procesado (Prio: %d)\n", 
                       GetCurrentThreadId(), work->event.sensorId, work->priority);
            }

            free(work);                       // Liberación de la memoria reservada para este evento específico
        }
    }
    return 0;
}

int main(void) {
    printf("==================================================\n");
    printf(" INICIANDO DISPATCHER (MODULO 3) - MIGUEL RIVAS \n");
    printf("==================================================\n\n");

    // --- 1. CONEXIÓN A LA MEMORIA COMPARTIDA CREADA POR EL BROKER ---
    
    // Abrimos el objeto de mapeo de memoria compartida global existente
    HANDLE hMapFile = OpenFileMappingA(
        FILE_MAP_ALL_ACCESS,        // Permisos completos de lectura y escritura
        FALSE,                      // No heredable por procesos hijos
        SHM_NAME                    // Nombre global definido en config.h
    );

    if (hMapFile == NULL) {
        printf("Error: No se pudo abrir la memoria compartida (¿Broker apagado?). Código: %lu\n", GetLastError());
        return 1;
    }

    // Mapeamos el archivo en el espacio de memoria virtual de nuestro proceso
    void* pBuf = MapViewOfFile(
        hMapFile,                   // Handle del mapeo abierto
        FILE_MAP_ALL_ACCESS,        // Permisos de lectura/escritura
        0, 0,                       // Desplazamiento inicial (0)
        SHM_SIZE                    // Tamaño total definido en common.h
    );

    if (pBuf == NULL) {
        printf("Error: No se pudo proyectar la vista de memoria. Código: %lu\n", GetLastError());
        CloseHandle(hMapFile);       // Liberamos el handle de mapeo antes de salir
        return 1;
    }

    // Mapeamos las cabeceras y el búfer circular usando aritmética de punteros en base al common.h
    SharedHeader* header = (SharedHeader*)pBuf;
    CircularBuffer* cb = (CircularBuffer*)((char*)pBuf + sizeof(SharedHeader));

    // --- 2. APERTURA DE OBJETOS DE SINCRONIZACIÓN GLOBALES ---
    
    // Abrimos el Mutex global de exclusión mutua para el búfer
    HANDLE hMutex = OpenMutexA(SYNCHRONIZE, FALSE, MUTEX_NAME);
    // Abrimos el semáforo que indica si hay elementos listos para ser leídos
    HANDLE hSemFull = OpenSemaphoreA(SEMAPHORE_ALL_ACCESS, FALSE, SEM_FULL_NAME);
    // Abrimos el semáforo que indica si hay espacios vacíos en el búfer
    HANDLE hSemEmpty = OpenSemaphoreA(SEMAPHORE_ALL_ACCESS, FALSE, SEM_EMPTY_NAME);

    // Validación estricta de que todos los objetos de sincronización existan
    if (hMutex == NULL || hSemFull == NULL || hSemEmpty == NULL) {
        printf("Error: Fallo al abrir los objetos de sincronización. Código: %lu\n", GetLastError());
        UnmapViewOfFile(pBuf);       // Desenlazamos la vista de la memoria
        CloseHandle(hMapFile);       // Cerramos el mapeo de archivo
        return 1;
    }

    // --- 3. CREACIÓN DEL ARCHIVO DE LOG COMPARTIDO ---
    
    // Creamos o abrimos el archivo de log con acceso concurrente para escritura compartida
    hLogFile = CreateFileA(
        LOG_FILE_NAME,                // Nombre físico del archivo en disco
        GENERIC_WRITE,                // Permisos de escritura
        FILE_SHARE_WRITE,             // Habilita la compartición de escritura entre hilos/procesos
        NULL,                         // Atributos de seguridad por defecto
        OPEN_ALWAYS,                  // Lo crea si no existe; si existe, lo abre
        FILE_ATTRIBUTE_NORMAL,        // Atributos normales de archivo
        NULL                          // Sin archivo de plantilla
    );

    if (hLogFile == INVALID_HANDLE_VALUE) {
        printf("Error: No se pudo crear el archivo de log. Código: %lu\n", GetLastError());
        // Liberación escalonada de recursos antes de salir
        UnmapViewOfFile(pBuf);
        CloseHandle(hMutex); CloseHandle(hSemFull); CloseHandle(hSemEmpty); CloseHandle(hMapFile);
        return 1;
    }

    // --- 4. CONFIGURACIÓN DEL PUERTO DE FINALIZACIÓN (IOCP) ---
    
    // Creamos el IOCP que funcionará como nuestra cola de tareas ultrarrápida
    hIOCP = CreateIoCompletionPort(
        INVALID_HANDLE_VALUE,         // Sin archivo físico asociado (solo cola en memoria)
        NULL,                         // Sin IOCP existente previo
        0,                            // Sin clave de completación
        NUM_WORKERS                   // Límite de hilos concurrentes que admite
    );

    if (hIOCP == NULL) {
        printf("Error: No se pudo crear el puerto de finalización (IOCP). Código: %lu\n", GetLastError());
        CloseHandle(hLogFile); UnmapViewOfFile(pBuf);
        CloseHandle(hMutex); CloseHandle(hSemFull); CloseHandle(hSemEmpty); CloseHandle(hMapFile);
        return 1;
    }

    // --- 5. CREACIÓN DEL POOL DE TRABAJADORES (WORKERS) ---
    
    HANDLE hWorkers[NUM_WORKERS];     // Arreglo para guardar los Handles de los hilos creados
    for (int i = 0; i < NUM_WORKERS; i++) {
        // Lanzamos cada hilo apuntando a la función WorkerThread
        hWorkers[i] = CreateThread(NULL, 0, WorkerThread, NULL, 0, NULL);
        if (hWorkers[i] == NULL) {
            printf("Error al crear el hilo Worker %d. Código: %lu\n", i, GetLastError());
        }
    }

    printf("Conectado a Memoria Compartida de forma exitosa.\n");
    printf("Pool de %d hilos trabajadores inicializado. Esperando eventos...\n\n", NUM_WORKERS);

    // --- 6. BUCLE PRINCIPAL DEL DISPATCHER (CONSUMO Y REDIRECCIÓN) ---
    
    // El bucle continuará mientras el flag 'running' del encabezado de la memoria sea verdadero
    while (header->running) {
        // Esperamos pasivamente a que el Broker inserte un elemento (SemFull > 0)
        DWORD waitResult = WaitForSingleObject(hSemFull, 1000); // 1 segundo de timeout para revisar 'running'

        if (waitResult == WAIT_TIMEOUT) {
            continue; // Si expira el tiempo, vuelve al inicio para verificar si 'running' cambió
        }

        if (waitResult == WAIT_OBJECT_0) {
            // Reclamamos el Mutex para modificar los índices del búfer de forma exclusiva
            WaitForSingleObject(hMutex, INFINITE);

            // Reservamos memoria dinámica para estructurar el trabajo que enviaremos al IOCP
            WorkItem* work = (WorkItem*)malloc(sizeof(WorkItem));
            if (work != NULL) {
                // Copiamos el evento desde la posición indicada por 'tail'
                work->event = cb->events[cb->tail];

                // --- CLASIFICACIÓN DE PRIORIDADES ---
                // Si el evento es tipo 1 (e.g. Frenos/Motor) es Crítico (1), el resto es Normal (2)
                if (work->event.eventType == 1) {
                    work->priority = 1;
                } else {
                    work->priority = 2;
                }

                // Desplazamos de forma circular el índice de lectura (tail)
                cb->tail = (cb->tail + 1) % BUFFER_SIZE;
                // Decrementamos de forma segura la cantidad de eventos pendientes en el búfer
                InterlockedDecrement(&cb->count);

                // Liberamos el Mutex para que el Broker pueda acceder de nuevo al búfer
                ReleaseMutex(hMutex);

                // Señalamos un espacio libre en el búfer, permitiendo ingreso de más datos (Backpressure)
                ReleaseSemaphore(hSemEmpty, 1, NULL);

                // Colocamos el paquete de trabajo en la cola de tareas del IOCP
                PostQueuedCompletionStatus(
                    hIOCP,                    // Handle del IOCP
                    sizeof(WorkItem),         // Tamaño del paquete enviado
                    0,                        // Sin clave de completación específica
                    (LPOVERLAPPED)work        // Puntero a la estructura enviada
                );
            } else {
                // Si falló el malloc, liberamos el mutex para evitar bloqueos mutuos permanentes
                ReleaseMutex(hMutex);
                // Devolvemos el semáforo para no perder el recurso
                ReleaseSemaphore(hSemFull, 1, NULL);
            }
        }
    }

    // --- 7. PROTOCOLO DE APAGADO LIMPIO (SHUTDOWN) ---
    
    printf("\nIniciando protocolo de apagado controlado...\n");

    // Enviamos a cada hilo del pool una señal de salida especial a través del IOCP
    for (int i = 0; i < NUM_WORKERS; i++) {
        WorkItem* exitWork = (WorkItem*)malloc(sizeof(WorkItem));
        if (exitWork != NULL) {
            exitWork->priority = EXIT_CODE_FLAG; // Código de apagado
            PostQueuedCompletionStatus(hIOCP, 0, 0, (LPOVERLAPPED)exitWork);
        }
    }

    // Esperamos pacientemente a que todos los hilos del pool terminen de procesar
    WaitForMultipleObjects(NUM_WORKERS, hWorkers, TRUE, INFINITE);

    // Cerramos los handles de todos los hilos trabajadores
    for (int i = 0; i < NUM_WORKERS; i++) {
        if (hWorkers[i] != NULL) {
            CloseHandle(hWorkers[i]);
        }
    }

    // --- LIBERACIÓN DE RECURSOS DEL SISTEMA OPERATIVO (handles de Win32) ---
    CloseHandle(hIOCP);               // Cerramos la cola del IOCP
    CloseHandle(hLogFile);             // Cerramos el archivo de Log en disco
    CloseHandle(hMutex);               // Cerramos el Mutex de memoria compartida
    CloseHandle(hSemFull);             // Cerramos el Semáforo de llenos
    CloseHandle(hSemEmpty);            // Cerramos el Semáforo de vacíos
    UnmapViewOfFile(pBuf);             // Desenlazamos la vista de la memoria compartida
    CloseHandle(hMapFile);             // Cerramos el handle principal del mapeo de memoria

    printf("Dispatcher finalizado limpiamente. Sin fugas de recursos.\n");
    return 0;
}