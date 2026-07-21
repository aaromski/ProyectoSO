<<<<<<< HEAD
=======

>>>>>>> ee5aec23fe198d3c14acfdd0542731e95a123c9e
/******************************************************************************
 * NOMBRE: Angie Urrieta
 * PROYECTO: Sistema de Telemetría y Control Concurrente (Fórmula 1) - MÓDULO 1
 * DESCRIPCIÓN: Subsistema de Sensores (Procesos Independientes). Simula
 *              sensores físicos del vehículo (Motor, Neumáticos, Frenos, GPS).
 *              Genera eventos con ID único, timestamp de alta resolución y
 *              payload aleatorio. Los envía al Broker mediante Named Pipe
 *              configurado en modo bloqueante.
 * FECHA: 17 de julio de 2026
 *****************************************************************************/

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
<<<<<<< HEAD
#include "common.h"

/* ================================================================
   CONSTANTES
   ================================================================ */
#define PIPE_NAME "\\\\.\\pipe\\SensorPipe"
=======

#define PIPE_NAME "\\\\.\\pipe\\SensorPipe"

/* ================================================================
   ESTRUCTURA DEL EVENTO (COMPATIBLE CON common.h)
   ================================================================ */
typedef struct {
    char    sensorId[32];
    char    timestamp[64];
    int     eventType;
    double  value;
} Event;
>>>>>>> ee5aec23fe198d3c14acfdd0542731e95a123c9e

/* ================================================================
   VARIABLES GLOBALES
   ================================================================ */
static HANDLE g_hPipe = INVALID_HANDLE_VALUE;
static volatile LONG g_Running = 1;
<<<<<<< HEAD
static volatile LONG g_EventCounter = 0;   /* Contador global de eventos */

/* ================================================================
   FUNCIÓN: Obtener timestamp de ALTA RESOLUCIÓN (microsegundos)
   ================================================================ */
static void ObtenerTimestamp(char* buffer, int tamaño) {
    LARGE_INTEGER freq, counter;
    SYSTEMTIME st;
    double seconds, milliseconds;

    /* Obtener frecuencia del contador de alta resolución */
    if (!QueryPerformanceFrequency(&freq)) {
        /* Fallback a GetLocalTime si QueryPerformanceCounter no está disponible */
        GetLocalTime(&st);
        snprintf(buffer, tamaño,
                 "%04d-%02d-%02d %02d:%02d:%02d.%03d",
                 st.wYear, st.wMonth, st.wDay,
                 st.wHour, st.wMinute, st.wSecond,
                 st.wMilliseconds);
        return;
    }

    /* Obtener contador de alta resolución */
    QueryPerformanceCounter(&counter);

    /* Calcular tiempo en segundos y milisegundos */
    seconds = (double)counter.QuadPart / (double)freq.QuadPart;
    milliseconds = seconds * 1000.0;

    /* Obtener fecha/hora actual para el formato legible */
    GetLocalTime(&st);

    /* Formatear con milisegundos de alta resolución */
=======

/* ================================================================
   FUNCIÓN: Obtener timestamp de alta resolución
   ================================================================ */
static void ObtenerTimestamp(char* buffer, int tamaño) {
    SYSTEMTIME st;
    GetLocalTime(&st);
>>>>>>> ee5aec23fe198d3c14acfdd0542731e95a123c9e
    snprintf(buffer, tamaño,
             "%04d-%02d-%02d %02d:%02d:%02d.%03d",
             st.wYear, st.wMonth, st.wDay,
             st.wHour, st.wMinute, st.wSecond,
<<<<<<< HEAD
             (int)(milliseconds) % 1000);
}

/* ================================================================
   FUNCIÓN: Generar valor aleatorio (payload)
   ================================================================ */
static double GenerarValor(void) {
    return (rand() % 1000) / 10.0;  /* 0.0 a 99.9 */
}

/* ================================================================
   FUNCIÓN: Generar tipo de evento aleatorio
   ================================================================ */
static int GenerarTipoEvento(void) {
    return (rand() % 3) + 1;  /* 1: Motor, 2: Frenos, 3: GPS */
=======
             st.wMilliseconds);
}

/* ================================================================
   FUNCION: Generar valor aleatorio (payload)
   ================================================================ */
static double GenerarValor(void) {
    return (rand() % 1000) / 10.0;  // 0.0 a 99.9
}

/* ================================================================
   FUNCION: Generar tipo de evento aleatorio
   ================================================================ */
static int GenerarTipoEvento(void) {
    return (rand() % 3) + 1;  // 1: Motor, 2: Frenos, 3: GPS
>>>>>>> ee5aec23fe198d3c14acfdd0542731e95a123c9e
}

/* ================================================================
   FUNCIÓN: Conectar al Named Pipe del Broker
   ================================================================ */
static BOOL ConectarAlBroker(void) {
    g_hPipe = CreateFileA(
        PIPE_NAME,
        GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (g_hPipe == INVALID_HANDLE_VALUE) {
        printf("[Sensor] ERROR: No se pudo conectar al Broker.\n");
<<<<<<< HEAD
        printf("[Sensor] Asegurate de que broker.exe este ejecutandose.\n");
        return FALSE;
    }

    /* Configurar pipe en modo bloqueante (por defecto ya lo está) */
    DWORD pipeMode = PIPE_READMODE_BYTE | PIPE_WAIT;
    if (!SetNamedPipeHandleState(g_hPipe, &pipeMode, NULL, NULL)) {
        printf("[Sensor] ADVERTENCIA: No se pudo configurar pipe en modo bloqueante.\n");
    }

=======
        printf("[Sensor] Asegúrate de que broker.exe esté ejecutandose.\n");
        return FALSE;
    }

>>>>>>> ee5aec23fe198d3c14acfdd0542731e95a123c9e
    printf("[Sensor] Conectado al Broker exitosamente.\n");
    return TRUE;
}

/* ================================================================
   FUNCIÓN: Enviar evento por el Named Pipe
   ================================================================ */
static BOOL EnviarEvento(const Event* evento) {
    DWORD bytesEscritos;

    if (!WriteFile(g_hPipe, evento, sizeof(Event), &bytesEscritos, NULL)) {
        printf("[Sensor] ERROR al enviar evento: %lu\n", GetLastError());
        return FALSE;
    }

<<<<<<< HEAD
    if (bytesEscritos != sizeof(Event)) {
        printf("[Sensor] ADVERTENCIA: Bytes escritos (%lu) != tamaño esperado (%llu)\n",
               bytesEscritos, (unsigned long long)sizeof(Event));
        return FALSE;
    }

=======
>>>>>>> ee5aec23fe198d3c14acfdd0542731e95a123c9e
    return TRUE;
}

/* ================================================================
   FUNCIÓN: Liberar recursos (cierre limpio de handles)
   ================================================================ */
static void LimpiarRecursos(void) {
    if (g_hPipe != INVALID_HANDLE_VALUE) {
        FlushFileBuffers(g_hPipe);
        CloseHandle(g_hPipe);
        g_hPipe = INVALID_HANDLE_VALUE;
        printf("[Sensor] Pipe cerrado correctamente.\n");
    }
}

/* ================================================================
<<<<<<< HEAD
   MANEJADOR DE CIERRE ORDENADO (Ctrl+C, Ctrl+Break, Cierre de consola)
   ================================================================ */
static BOOL WINAPI ManejadorCtrl(DWORD evento) {
    switch (evento) {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_CLOSE_EVENT:   /* Usuario cierra la ventana con la X */
            printf("\n\n[Sensor] Señal de apagado recibida (evento: %lu).\n", evento);
            printf("[Sensor] Iniciando protocolo de cierre limpio...\n");

            g_Running = 0;
            LimpiarRecursos();

            printf("[Sensor] Finalizado correctamente.\n");

            /* Para CTRL_CLOSE_EVENT, no llamar a exit() porque el sistema ya está cerrando */
            if (evento != CTRL_CLOSE_EVENT) {
                exit(0);
            }
            return TRUE;

        default:
            return FALSE;
    }
=======
   MANEJADOR DE (CIERRE ORDENADO)
   ================================================================ */
static BOOL WINAPI ManejadorCtrl(DWORD evento) {
    if (evento == CTRL_C_EVENT || evento == CTRL_BREAK_EVENT) {
        printf("\n\n[Sensor] Señal de apagado recibida.\n");
        printf("[Sensor] Iniciando protocolo de cierre limpio...\n");

        g_Running = 0;
        LimpiarRecursos();

        printf("[Sensor] Finalizado correctamente.\n");
        exit(0);
        return TRUE;
    }
    return FALSE;
>>>>>>> ee5aec23fe198d3c14acfdd0542731e95a123c9e
}

/* ================================================================
   FUNCIÓN PRINCIPAL
   ================================================================ */
int main(int argc, char* argv[]) {
<<<<<<< HEAD
    char sensorId[SENSOR_ID_LEN];
=======
    char sensorId[32];
>>>>>>> ee5aec23fe198d3c14acfdd0542731e95a123c9e
    Event evento;
    int contadorEventos = 0;

    /* ============================================================
       CONFIGURACIÓN INICIAL
       ============================================================ */

<<<<<<< HEAD
    /* Configurar manejador de Ctrl+C y cierre de consola */
    SetConsoleCtrlHandler(ManejadorCtrl, TRUE);

    /* Inicializar generador de números aleatorios */
    srand((unsigned)time(NULL));

    /* Obtener ID del sensor desde argumentos */
=======
    // Configurar manejador de Ctrl+C
    SetConsoleCtrlHandler(ManejadorCtrl, TRUE);

    // Inicializar generador de números aleatorios
    srand((unsigned)time(NULL));

    // Obtener ID del sensor desde argumentos
>>>>>>> ee5aec23fe198d3c14acfdd0542731e95a123c9e
    if (argc >= 2) {
        strncpy(sensorId, argv[1], sizeof(sensorId) - 1);
        sensorId[sizeof(sensorId) - 1] = '\0';
    } else {
        strcpy(sensorId, "SENSOR_DEFAULT");
        printf("[Sensor] Usando ID por defecto. Pasa un argumento: sensor.exe Motor\n");
    }

    printf("============================================================\n");
    printf(" SENSOR DE TELEMETRIA - MODULO 1\n");
    printf(" Nombre: Angie Urrieta\n");
    printf(" ID del Sensor: %s\n", sensorId);
<<<<<<< HEAD
    printf(" Tamano de Event: %llu bytes (debe coincidir con broker)\n",
           (unsigned long long)sizeof(Event));
=======
>>>>>>> ee5aec23fe198d3c14acfdd0542731e95a123c9e
    printf("============================================================\n\n");

    /* ============================================================
       CONEXIÓN AL BROKER
       ============================================================ */

    if (!ConectarAlBroker()) {
        printf("[Sensor] Presiona Enter para salir...");
        getchar();
        return 1;
    }

    printf("[Sensor] Enviando eventos cada 1 segundo.\n");
    printf("[Sensor] Presiona Ctrl+C para finalizar.\n\n");

    /* ============================================================
       BUCLE PRINCIPAL: GENERAR Y ENVIAR EVENTOS
       ============================================================ */

    while (g_Running) {
        contadorEventos++;
<<<<<<< HEAD
        LONG eventId = InterlockedIncrement(&g_EventCounter);

        /* ---- 1. ID del sensor ---- */
        strcpy(evento.sensorId, sensorId);

        /* ---- 2. Timestamp de ALTA RESOLUCIÓN ---- */
        ObtenerTimestamp(evento.timestamp, sizeof(evento.timestamp));

        /* ---- 3. Tipo de evento aleatorio (1: Motor, 2: Frenos, 3: GPS) ---- */
        evento.eventType = GenerarTipoEvento();

        /* ---- 4. Valor aleatorio (payload) ---- */
        evento.value = GenerarValor();

        /* Mostrar en consola (incluyendo el ID único del evento) */
        printf("[Sensor] #%d (ID:%ld) | Sensor: %s | Time: %s | Type: %d | Value: %.2f\n",
               contadorEventos,
               eventId,
=======

        // ID del sensor
        strcpy(evento.sensorId, sensorId);

        // Timestamp de alta resolución
        ObtenerTimestamp(evento.timestamp, sizeof(evento.timestamp));

        // Tipo de evento aleatorio (1: Motor, 2: Frenos, 3: GPS)
        evento.eventType = GenerarTipoEvento();

        // Valor aleatorio (payload)
        evento.value = GenerarValor();

        // Mostrar en consola
        printf("[Sensor] #%d | ID: %s | Time: %s | Type: %d | Value: %.2f\n",
               contadorEventos,
>>>>>>> ee5aec23fe198d3c14acfdd0542731e95a123c9e
               evento.sensorId,
               evento.timestamp,
               evento.eventType,
               evento.value);

<<<<<<< HEAD
        /* Enviar por Named Pipe (bloqueante) */
=======
        // Enviar por Named Pipe (bloqueante)
>>>>>>> ee5aec23fe198d3c14acfdd0542731e95a123c9e
        if (!EnviarEvento(&evento)) {
            printf("[Sensor] Error en envio. Cerrando...\n");
            break;
        }

<<<<<<< HEAD
        /* Esperar 1 segundo (frecuencia de muestreo) */
=======
        // Esperar 1 segundo (frecuencia de muestreo)
>>>>>>> ee5aec23fe198d3c14acfdd0542731e95a123c9e
        Sleep(1000);
    }

    /* ============================================================
       CIERRE LIMPIO
       ============================================================ */

    LimpiarRecursos();
    printf("[Sensor] Finalizado correctamente.\n");
    return 0;
}
