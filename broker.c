/******************************************************************************
 * NOMBRE: Aarom Luces
 * PROYECTO: Sistema de Telemetría y Control Concurrente (Fórmula 1) - MÓDULO 2
 * DESCRIPCIÓN: Núcleo receptor e ingestor central. Gestiona conexiones
 *              entrantes de sensores mediante Named Pipes distribuidos en hilos
 *              dedicados (CreateThread) y deposita los eventos en un Buffer
 *              Circular en Memoria Compartida protegido por Semáforos y Mutex.
 * FECHA: 12 de julio de 2026
 *****************************************************************************/

#include <windows.h>
#include <stdio.h>
#include "common.h"
#include "config.h"

 /* Eventos de comunicación con Módulo 4 (Monitor) */
#define EVENT_SHUTDOWN_NAME "BrokerShutdownEvent"
#define EVENT_DEBUG_NAME    "BrokerDebugDumpEvent"

/* ================================================================
   VARIABLES GLOBALES DEL BROKER
   ================================================================ */

static HANDLE          g_hMapFile = NULL;
static LPVOID          g_pMem = NULL;
static SharedHeader* g_pHeader = NULL;
static CircularBuffer* g_pBuffer = NULL;

static HANDLE          g_hMutex = NULL;
static HANDLE          g_hSemEmpty = NULL;
static HANDLE          g_hSemFull = NULL;

/* Handles para el Monitor (Módulo 4) */
static HANDLE          g_hEvtShutdown = NULL;
static HANDLE          g_hEvtDebug = NULL;
static HANDLE          g_hMonitorThread = NULL;

static HANDLE          g_hThreads[MAX_CLIENTS];
static HANDLE          g_hPipes[MAX_CLIENTS];
static volatile LONG   g_Running = 1;
static volatile LONG   g_SensorCount = 0;

/* ================================================================
   FUNCIONES AUXILIARES
   ================================================================ */

static void Log(const char* msg) {
    printf("[Broker] %s\n", msg);
}

/* Declaración anticipada */
static DWORD WINAPI ShutdownThread(LPVOID lpParam);

/* ================================================================
   INICIALIZACION: MEMORIA COMPARTIDA (FILE MAPPING)
   ================================================================ */

static BOOL InitSharedMemory(void) {
    g_hMapFile = CreateFileMappingA(
        INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, SHM_SIZE, SHM_NAME);
    if (!g_hMapFile) {
        Log("ERROR: CreateFileMapping fallo");
        return FALSE;
    }

    g_pMem = MapViewOfFile(g_hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, SHM_SIZE);
    if (!g_pMem) {
        Log("ERROR: MapViewOfFile fallo");
        return FALSE;
    }

    g_pHeader = (SharedHeader*)g_pMem;
    g_pHeader->bufferOffset = sizeof(SharedHeader);
    g_pHeader->bufferMaxSize = BUFFER_SIZE;
    g_pHeader->activeSensors = 0;
    g_pHeader->running = 1;

    g_pBuffer = (CircularBuffer*)((char*)g_pMem + sizeof(SharedHeader));
    g_pBuffer->head = 0;
    g_pBuffer->tail = 0;
    g_pBuffer->count = 0;
    g_pBuffer->maxSize = BUFFER_SIZE;

    Log("Memoria compartida inicializada");
    return TRUE;
}

/* ================================================================
   INICIALIZACION: OBJETOS DE SINCRONIZACION Y EVENTOS
   ================================================================ */

static BOOL InitSyncObjects(void) {
    g_hMutex = CreateMutexA(NULL, FALSE, MUTEX_NAME);
    if (!g_hMutex) {
        Log("ERROR: CreateMutex fallo");
        return FALSE;
    }

    g_hSemEmpty = CreateSemaphoreA(NULL, BUFFER_SIZE, BUFFER_SIZE, SEM_EMPTY_NAME);
    if (!g_hSemEmpty) {
        Log("ERROR: CreateSemaphore (empty) fallo");
        return FALSE;
    }

    g_hSemFull = CreateSemaphoreA(NULL, 0, BUFFER_SIZE, SEM_FULL_NAME);
    if (!g_hSemFull) {
        Log("ERROR: CreateSemaphore (full) fallo");
        return FALSE;
    }

    /* Eventos para el Módulo 4 */
    g_hEvtShutdown = CreateEventA(NULL, TRUE, FALSE, EVENT_SHUTDOWN_NAME);
    g_hEvtDebug = CreateEventA(NULL, TRUE, FALSE, EVENT_DEBUG_NAME);

    if (!g_hEvtShutdown || !g_hEvtDebug) {
        Log("ERROR: Fallo al crear Eventos de sincronizacion");
        return FALSE;
    }

    Log("Objetos de sincronizacion y eventos creados");
    return TRUE;
}

/* ================================================================
   VOLCADO DE DEPURACION (DEBUG DUMP)
   ================================================================ */

static void DumpDebugInfo(void) {
    char buf[128];
    Log("--- VOLCADO DE DEPURACION (SOLICITADO POR MONITOR) ---");
    if (g_pHeader && g_pBuffer) {
        snprintf(buf, sizeof(buf), "  > Sensores activos : %ld", g_pHeader->activeSensors);
        Log(buf);
        snprintf(buf, sizeof(buf), "  > Eventos en Buffer: %ld / %ld", g_pBuffer->count, g_pBuffer->maxSize);
        Log(buf);
        snprintf(buf, sizeof(buf), "  > Indice Head (Lectura) : %ld", g_pBuffer->head);
        Log(buf);
        snprintf(buf, sizeof(buf), "  > Indice Tail (Escritura): %ld", g_pBuffer->tail);
        Log(buf);
    }
    Log("-----------------------------------------------------");
}

/* ================================================================
   HILO ESCUCHADOR DE EVENTOS DEL MONITOR (MODULO 4)
   ================================================================ */

static DWORD WINAPI MonitorListenerThread(LPVOID lpParam) {
    (void)lpParam;
    HANDLE handles[2] = { g_hEvtShutdown, g_hEvtDebug };

    while (g_Running) {
        DWORD waitRes = WaitForMultipleObjects(2, handles, FALSE, 1000);

        if (waitRes == WAIT_OBJECT_0) {
            Log("Recibida orden de apagado desde el Monitor (Modulo 4)");
            ResetEvent(g_hEvtShutdown);
            CreateThread(NULL, 0, ShutdownThread, NULL, 0, NULL);
            break;
        }
        else if (waitRes == WAIT_OBJECT_0 + 1) {
            ResetEvent(g_hEvtDebug);
            DumpDebugInfo();
        }
    }
    return 0;
}

/* ================================================================
   HILO RECEPTOR: LEE DEL PIPE, INSERTA EN BUFFER CIRCULAR
   ================================================================ */

static DWORD WINAPI ReceiverThread(LPVOID lpParam) {
    int   slot = (int)(INT_PTR)lpParam;
    HANDLE hPipe = g_hPipes[slot];
    Event  evt;
    DWORD  bytesRead;

    InterlockedIncrement(&g_pHeader->activeSensors);
    Log("Hilo receptor iniciado");

    while (g_Running) {
        BOOL ok = ReadFile(hPipe, &evt, sizeof(Event), &bytesRead, NULL);
        if (!ok || bytesRead == 0)
            break;

        if (bytesRead != sizeof(Event))
            continue;

        WaitForSingleObject(g_hSemEmpty, INFINITE);
        if (!g_Running) break;

        WaitForSingleObject(g_hMutex, INFINITE);

        g_pBuffer->events[g_pBuffer->tail] = evt;
        g_pBuffer->tail = (g_pBuffer->tail + 1) % BUFFER_SIZE;
        InterlockedIncrement(&g_pBuffer->count);

        ReleaseMutex(g_hMutex);
        ReleaseSemaphore(g_hSemFull, 1, NULL);

        Log("Evento insertado en buffer circular");
    }

    InterlockedDecrement(&g_pHeader->activeSensors);
    FlushFileBuffers(hPipe);
    DisconnectNamedPipe(hPipe);
    CloseHandle(hPipe);
    g_hPipes[slot] = INVALID_HANDLE_VALUE;

    Log("Hilo receptor finalizado");
    return 0;
}

/* ================================================================
   LIBERACION DE RECURSOS
   ================================================================ */

static void Cleanup(void) {
    if (g_hEvtShutdown) { CloseHandle(g_hEvtShutdown); g_hEvtShutdown = NULL; }
    if (g_hEvtDebug) { CloseHandle(g_hEvtDebug);    g_hEvtDebug = NULL; }

    if (g_hMutex) {
        CloseHandle(g_hMutex);
        g_hMutex = NULL;
    }
    if (g_hSemEmpty) {
        CloseHandle(g_hSemEmpty);
        g_hSemEmpty = NULL;
    }
    if (g_hSemFull) {
        CloseHandle(g_hSemFull);
        g_hSemFull = NULL;
    }
    if (g_pMem) {
        UnmapViewOfFile(g_pMem);
        g_pMem = NULL;
    }
    if (g_hMapFile) {
        CloseHandle(g_hMapFile);
        g_hMapFile = NULL;
    }

    Log("Recursos liberados");
}

/* ================================================================
   CIERRE ORDENADO: HILO DE APAGADO
   ================================================================ */

static DWORD WINAPI ShutdownThread(LPVOID lpParam) {
    (void)lpParam;

    InterlockedExchange(&g_Running, 0);

    /* Notificar al Módulo 3 (Dispatcher) */
    if (g_pHeader) {
        g_pHeader->running = 0;
    }

    /* Destrabar semáforos si hay hilos esperando */
    ReleaseSemaphore(g_hSemFull, 1, NULL);

    for (long i = 0; i < g_SensorCount; i++) {
        if (g_hPipes[i] != INVALID_HANDLE_VALUE)
            CancelIoEx(g_hPipes[i], NULL);
    }

    for (long i = 0; i < g_SensorCount; i++) {
        if (g_hThreads[i]) {
            WaitForSingleObject(g_hThreads[i], 2000);
            CloseHandle(g_hThreads[i]);
        }
    }

    for (long i = 0; i < g_SensorCount; i++) {
        if (g_hPipes[i] != INVALID_HANDLE_VALUE) {
            DisconnectNamedPipe(g_hPipes[i]);
            CloseHandle(g_hPipes[i]);
            g_hPipes[i] = INVALID_HANDLE_VALUE;
        }
    }

    Cleanup();
    Log("Apagado completo");
    ExitProcess(0);
    return 0;
}

/* ================================================================
   CONSOLA: CTRL+C / CTRL+BREAK
   ================================================================ */

static BOOL WINAPI ConsoleHandler(DWORD event) {
    if (event == CTRL_C_EVENT || event == CTRL_BREAK_EVENT) {
        Log("Senal de apagado recibida por teclado...");
        CreateThread(NULL, 0, ShutdownThread, NULL, 0, NULL);
        return TRUE;
    }
    return FALSE;
}

/* ================================================================
   FUNCION PRINCIPAL
   ================================================================ */

int main(void) {
    SetConsoleCtrlHandler(ConsoleHandler, TRUE);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        g_hThreads[i] = NULL;
        g_hPipes[i] = INVALID_HANDLE_VALUE;
    }

    Log("Iniciando Broker...");

    if (!InitSharedMemory()) { Cleanup(); return 1; }
    if (!InitSyncObjects()) { Cleanup(); return 1; }

    /* Crear hilo para responder al Monitor */
    g_hMonitorThread = CreateThread(NULL, 0, MonitorListenerThread, NULL, 0, NULL);

    Log("Broker listo. Esperando sensores...");

    while (g_Running) {
        HANDLE hPipe = CreateNamedPipeA(
            PIPE_NAME,
            PIPE_ACCESS_INBOUND,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES,
            sizeof(Event) * BUFFER_SIZE,
            sizeof(Event),
            0,
            NULL);

        if (hPipe == INVALID_HANDLE_VALUE) {
            if (!g_Running) break;
            Log("ERROR: CreateNamedPipe fallo");
            break;
        }

        BOOL connected = ConnectNamedPipe(hPipe, NULL);
        if (!connected && GetLastError() != ERROR_PIPE_CONNECTED) {
            CloseHandle(hPipe);
            continue;
        }

        if (!g_Running) {
            CloseHandle(hPipe);
            break;
        }

        long slot = InterlockedIncrement(&g_SensorCount) - 1;
        if (slot >= MAX_CLIENTS) {
            InterlockedDecrement(&g_SensorCount);
            DisconnectNamedPipe(hPipe);
            CloseHandle(hPipe);
            Log("Maximo de sensores alcanzado");
            continue;
        }

        g_hPipes[slot] = hPipe;
        g_hThreads[slot] = CreateThread(
            NULL, 0, ReceiverThread, (LPVOID)(INT_PTR)slot, 0, NULL);

        if (!g_hThreads[slot]) {
            DisconnectNamedPipe(hPipe);
            CloseHandle(hPipe);
            g_hPipes[slot] = INVALID_HANDLE_VALUE;
            InterlockedDecrement(&g_SensorCount);
            Log("ERROR: CreateThread fallo");
        }
        else {
            Log("Sensor conectado");
        }
    }

    Cleanup();
    return 0;
}
