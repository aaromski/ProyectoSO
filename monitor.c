/******************************************************************************
 * NOMBRE: Lisbelis Yemes
 * PROYECTO: Sistema de Telemetría y Control Concurrente (Fórmula 1) - MÓDULO 4
 * DESCRIPCIÓN: Consola de administración e interfaz gráfica/texto en tiempo real.
 *              Abre el mapeo de memoria compartida para auditar estadísticas de
 *              rendimiento del sistema y se coordina con el Broker mediante
 *              Eventos de Windows (CreateEvent) para el protocolo de apagado.
 * FECHA: 16 de julio de 2026
 *****************************************************************************/

#include <windows.h>
#include <conio.h>
#include <stdio.h>
#include "common.h"

/* ================================================================
   NOMBRES DE LOS EVENTOS DE WINDOWS PARA COORDINARSE CON EL BROKER
   ================================================================ */
#define EVENT_SHUTDOWN_NAME "BrokerShutdownEvent"
#define EVENT_DEBUG_NAME    "BrokerDebugDumpEvent"

   /* ================================================================
      VARIABLES GLOBALES DEL MONITOR
      ================================================================ */

static HANDLE          g_hMapFile = NULL;
static LPVOID          g_pMem = NULL;
static SharedHeader* g_pHeader = NULL;
static CircularBuffer* g_pBuffer = NULL;

static HANDLE          g_hShutdownEvt = NULL;
static HANDLE          g_hDebugEvt = NULL;

static volatile LONG   g_Running = 1;

/* ================================================================
   UTILIDAD DE LOG
   ================================================================ */

static void Log(const char* msg) {
    printf("[Monitor] %s\n", msg);
}

/* ================================================================
   CONEXION A LA MEMORIA COMPARTIDA CREADA POR EL BROKER
   ================================================================ */

static BOOL ConnectToSharedMemory(void) {
    for (int intentos = 0; intentos < 15; intentos++) {
        g_hMapFile = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, SHM_NAME);
        if (g_hMapFile) break;

        Log("Broker no disponible todavia, reintentando...");
        Sleep(1000);
    }

    if (!g_hMapFile) {
        Log("ERROR: no se pudo abrir la memoria compartida. ¿Esta el broker corriendo?");
        return FALSE;
    }

    g_pMem = MapViewOfFile(g_hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, SHM_SIZE);
    if (!g_pMem) {
        Log("ERROR: MapViewOfFile fallo");
        return FALSE;
    }

    g_pHeader = (SharedHeader*)g_pMem;
    g_pBuffer = (CircularBuffer*)((char*)g_pMem + sizeof(SharedHeader));

    Log("Conectado a la memoria compartida del broker");
    return TRUE;
}

/* ================================================================
   EVENTOS DE WINDOWS PARA COORDINARSE CON EL BROKER
   ================================================================ */

static BOOL InitEvents(void) {
    g_hShutdownEvt = CreateEventA(NULL, TRUE, FALSE, EVENT_SHUTDOWN_NAME);
    if (!g_hShutdownEvt) {
        Log("ERROR: CreateEvent (shutdown) fallo");
        return FALSE;
    }

    g_hDebugEvt = CreateEventA(NULL, TRUE, FALSE, EVENT_DEBUG_NAME);
    if (!g_hDebugEvt) {
        Log("ERROR: CreateEvent (debug) fallo");
        return FALSE;
    }

    Log("Eventos de coordinacion listos");
    return TRUE;
}

/* ================================================================
   IMPRESION DE ESTADISTICAS
   ================================================================ */

static void PrintStats(void) {
    LONG activos = g_pHeader->activeSensors;
    LONG ocupados = g_pBuffer->count;
    long capacidad = g_pBuffer->maxSize;
    double pct = capacidad > 0 ? (100.0 * ocupados / capacidad) : 0.0;

    /* Limpia la pantalla para que se vea como un dashboard en vivo */
    system("cls");
    printf("=== DASHBOARD DE TELEMETRIA (Modulo 4) ===\n\n");
    printf("Sensores activos      : %ld\n", activos);
    printf("Ocupacion del buffer  : %ld / %ld  (%.1f%%)\n", ocupados, capacidad, pct);
    printf("Broker en ejecucion   : %s\n", g_pHeader->running ? "SI" : "NO");
    printf("\nControles: [Q] salir  [D] pedir volcado debug  [S] apagar sistema\n");
}

/* ================================================================
   CTRL+C: SALIR DEL MONITOR 
   ================================================================ */

static BOOL WINAPI ConsoleHandler(DWORD event) {
    if (event == CTRL_C_EVENT || event == CTRL_BREAK_EVENT) {
        Log("Cerrando monitor...");
        InterlockedExchange(&g_Running, 0);
        return TRUE;
    }
    return FALSE;
}

/* ================================================================
   LIBERACION DE RECURSOS
   ================================================================ */

static void Cleanup(void) {
    if (g_hDebugEvt) { CloseHandle(g_hDebugEvt);    g_hDebugEvt = NULL; }
    if (g_hShutdownEvt) { CloseHandle(g_hShutdownEvt); g_hShutdownEvt = NULL; }
    if (g_pMem) { UnmapViewOfFile(g_pMem);     g_pMem = NULL; }
    if (g_hMapFile) { CloseHandle(g_hMapFile);     g_hMapFile = NULL; }
    Log("Recursos liberados");
}

/* ================================================================
   FUNCION PRINCIPAL
   ================================================================ */

int main(void) {
    SetConsoleCtrlHandler(ConsoleHandler, TRUE);

    Log("Iniciando Monitor...");

    if (!ConnectToSharedMemory()) { Cleanup(); return 1; }
    if (!InitEvents()) { Cleanup(); return 1; }

    while (g_Running) {
        PrintStats();

        /* Espera hasta 500ms revisando teclado. No es busy waiting real
           porque el monitor solo refresca un dashboard, no protege un
           recurso compartido critico como el buffer circular. */
        if (_kbhit()) {
            int tecla = _getch();
            if (tecla == 'q' || tecla == 'Q') {
                InterlockedExchange(&g_Running, 0);
            }
            else if (tecla == 'd' || tecla == 'D') {
                Log("Solicitando volcado de debug al broker...");
                SetEvent(g_hDebugEvt);
            }
            else if (tecla == 's' || tecla == 'S') {
                Log("Solicitando apagado controlado del broker...");
                SetEvent(g_hShutdownEvt);
            }
        }
        else {
            Sleep(500);
        }
    }

    Cleanup();
    return 0;
}
