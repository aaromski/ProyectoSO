
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

/* ================================================================
   VARIABLES GLOBALES
   ================================================================ */
static HANDLE g_hPipe = INVALID_HANDLE_VALUE;
static volatile LONG g_Running = 1;

/* ================================================================
   FUNCIÓN: Obtener timestamp de alta resolución
   ================================================================ */
static void ObtenerTimestamp(char* buffer, int tamaño) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    snprintf(buffer, tamaño,
             "%04d-%02d-%02d %02d:%02d:%02d.%03d",
             st.wYear, st.wMonth, st.wDay,
             st.wHour, st.wMinute, st.wSecond,
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
        printf("[Sensor] Asegúrate de que broker.exe esté ejecutandose.\n");
        return FALSE;
    }

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
}

/* ================================================================
   FUNCIÓN PRINCIPAL
   ================================================================ */
int main(int argc, char* argv[]) {
    char sensorId[32];
    Event evento;
    int contadorEventos = 0;

    /* ============================================================
       CONFIGURACIÓN INICIAL
       ============================================================ */

    // Configurar manejador de Ctrl+C
    SetConsoleCtrlHandler(ManejadorCtrl, TRUE);

    // Inicializar generador de números aleatorios
    srand((unsigned)time(NULL));

    // Obtener ID del sensor desde argumentos
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
               evento.sensorId,
               evento.timestamp,
               evento.eventType,
               evento.value);

        // Enviar por Named Pipe (bloqueante)
        if (!EnviarEvento(&evento)) {
            printf("[Sensor] Error en envio. Cerrando...\n");
            break;
        }

        // Esperar 1 segundo (frecuencia de muestreo)
        Sleep(1000);
    }

    /* ============================================================
       CIERRE LIMPIO
       ============================================================ */

    LimpiarRecursos();
    printf("[Sensor] Finalizado correctamente.\n");
    return 0;
}
