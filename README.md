# ProyectoSO
Sistema de Telemetría Concurrente (Win32 API)
Este sistema implementa una arquitectura distribuida y concurrente en C para la recolección, almacenamiento intermedio y despacho de eventos de telemetría provenientes de múltiples sensores, utilizando exclusivamente las primitivas de sincronización de la API de Windows.

    REQUISITO CRÍTICO DE EJECUCIÓN: Debido al uso del espacio de nombres Global\ para la memoria compartida y los objetos de sincronización del sistema operativo, TODOS los ejecutables (.exe) deben iniciarse obligatoriamente haciendo clic derecho y seleccionando "Ejecutar como Administrador". De lo contrario, Windows denegará el acceso a los recursos y los módulos no podrán comunicarse.

    Secuencia de Arranque Obligatoria
Para garantizar que los recursos de memoria y los semáforos se creen y abran en el orden correcto, los módulos deben ejecutarse estrictamente en la siguiente secuencia:

broker.exe (Módulo 1 - Servidor Principal): Debe iniciarse primero, ya que es el encargado de reservar la memoria compartida (SharedHeader y CircularBuffer), el Mutex y los Semáforos (SemEmpty y SemFull), además de abrir la tubería con nombre (SensorPipe).

dispatcher.exe (Módulo 3 - Procesamiento): Se inicia segundo para abrir los handles de la memoria compartida y los semáforos creados por el Broker, quedando a la espera activa-pasiva de eventos para el pool de hilos trabajadores.

monitor.exe (Módulo 4 - Interfaz de Visualización): Se inicia tercero para conectarse al sistema y quedar listo para mostrar las alertas e informes procesados.

sensor.exe (Módulo 2 - Clientes de Telemetría): Se ejecutan al final. Pueden abrirse múltiples instancias en paralelo para simular el envío de ráfagas de datos hacia la tubería.

    Arquitectura del Sistema
El flujo de información está diseñado para evitar la espera activa y garantizar exclusión mutua mediante:

Ingesta de Datos: Comunicación por Tuberías con Nombre (Named Pipes) independientes por cada hilo de sensor en el Broker.

Almacenamiento Intermedio: Buffer circular en Memoria Compartida (Shared Memory) mapeado dinámicamente.

Sincronización: Control estricto de productores/consumidores mediante un Mutex global para el acceso al buffer y dos Semáforos (SemEmpty / SemFull) coordinando el flujo de datos.

Operaciones Atómicas: Uso de InterlockedIncrement e InterlockedDecrement para el control de variables concurrentes.

    👥 Equipo de Desarrollo
La estructura y los módulos del proyecto fueron desarrollados por:

Aarom Luces: Desarrollo del Módulo 1 (Broker).

Angie Urrieta: Desarrollo del Módulo 2 (Sensores).

Miguel Rivas: Desarrollo del Módulo 3 (Dispatcher).

Lisbelis Yemes: Desarrollo del Módulo 4 (Monitor).