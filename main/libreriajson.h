#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "cJSON.h"

// Variables globales principales
int max_jsons = 33;                     // Máximo de JSONs esperados (configurable)
int matriz_numeros[100][50];            // Matriz para almacenar números enteros [json_id][posicion]
int cantidad_numeros[100];              // Cuántos números tiene cada JSON
bool json_recibido[100];                // Array para marcar cuáles JSONs han sido recibidos
int total_jsons_recibidos = 0;          // Contador de JSONs recibidos
bool listo_para_imprimir = false;       // Flag para iniciar impresión

/**
 * Función para convertir cadena "1,2,3,4,15" en array de enteros
 */
int parsear_cadena_a_numeros(const char* cadena, int* array_numeros, int max_numeros) {
    if (cadena == NULL || strlen(cadena) == 0) {
        return 0;
    }
    
    char* cadena_copia = strdup(cadena);
    char* token;
    int count = 0;
    
    token = strtok(cadena_copia, ",");
    while (token != NULL && count < max_numeros) {
        // Limpiar espacios
        while (*token == ' ') token++;
        
        array_numeros[count] = atoi(token);
        count++;
        token = strtok(NULL, ",");
    }
    
    free(cadena_copia);
    return count;
}

/**
 * Función para almacenar JSON en la matriz de números
 */
void almacenar_json_en_matriz(const char* json_string) {
    cJSON *json = cJSON_Parse(json_string);
    if (json == NULL) {
        printf("Error: No se pudo parsear JSON\n");
        return;
    }

    // Extraer ID
    cJSON *id_item = cJSON_GetObjectItem(json, "id");
    if (!cJSON_IsNumber(id_item)) {
        printf("Error: Campo 'id' inválido\n");
        cJSON_Delete(json);
        return;
    }
    int id = id_item->valueint;

    // Verificar rango
    if (id < 1 || id > max_jsons) {
        printf("Error: ID %d fuera de rango (1-%d)\n", id, max_jsons);
        cJSON_Delete(json);
        return;
    }

    // Extraer línea
    cJSON *linea_item = cJSON_GetObjectItem(json, "linea");
    if (!cJSON_IsString(linea_item)) {
        printf("Error: Campo 'linea' inválido\n");
        cJSON_Delete(json);
        return;
    }

    int posicion = id - 1;  // Convertir a índice de array
    
    // Marcar como recibido
    if (!json_recibido[posicion]) {
        json_recibido[posicion] = true;
        total_jsons_recibidos++;
    }

    // Parsear números y almacenar en matriz_numeros
    cantidad_numeros[posicion] = parsear_cadena_a_numeros(
        linea_item->valuestring, 
        matriz_numeros[posicion], 
        50
    );
    
    printf("JSON %d procesado: %d números almacenados\n", id, cantidad_numeros[posicion]);
    printf("Total JSONs recibidos: %d/%d\n", total_jsons_recibidos, max_jsons);
    
    // Verificar si ya están todos completos
    if (total_jsons_recibidos >= max_jsons) {
        listo_para_imprimir = true;
        printf("¡SISTEMA LISTO PARA IMPRIMIR!\n");
    }
    
    cJSON_Delete(json);
}

/**
 * FUNCIÓN PRINCIPAL: Verificar y procesar mensajes UART
 * Esta es la función que debes llamar cuando recibas un mensaje por UART
 */
bool verificar_y_procesar_uart(const char* mensaje_uart) {
    // Validaciones básicas
    if (mensaje_uart == NULL || strlen(mensaje_uart) == 0) {
        printf("Error: Mensaje vacío\n");
        return false;
    }
    
    if (strchr(mensaje_uart, '{') == NULL || strchr(mensaje_uart, '}') == NULL) {
        printf("Error: No es formato JSON válido\n");
        return false;
    }
    
    if (strstr(mensaje_uart, "\"id\"") == NULL || strstr(mensaje_uart, "\"linea\"") == NULL) {
        printf("Error: JSON incompleto (falta 'id' o 'linea')\n");
        return false;
    }
    
    printf("JSON válido recibido: %s\n", mensaje_uart);
    
    // Procesar y almacenar
    almacenar_json_en_matriz(mensaje_uart);
    
    return true;
}

/**
 * Función para obtener un número específico por ID y posición
 * USO: obtener_numero(json_id, posicion_en_array)
 * Ejemplo: obtener_numero(1, 0) → primer número del JSON 1
 *          obtener_numero(1, 4) → quinto número del JSON 1
 */
int obtener_numero(int json_id, int posicion) {
    if (json_id < 1 || json_id > max_jsons) {
        printf("Error: ID %d fuera de rango\n", json_id);
        return -1;
    }
    
    int array_pos = json_id - 1;
    
    if (!json_recibido[array_pos]) {
        printf("Error: JSON %d no recibido\n", json_id);
        return -1;
    }
    
    if (posicion >= cantidad_numeros[array_pos] || posicion < 0) {
        printf("Error: Posición %d inválida para JSON %d\n", posicion, json_id);
        return -1;
    }
    
    return matriz_numeros[array_pos][posicion];
}

/**
 * Función para obtener cuántos números tiene un JSON específico
 */
int obtener_cantidad_numeros(int json_id) {
    if (json_id < 1 || json_id > max_jsons) return 0;
    
    int array_pos = json_id - 1;
    if (!json_recibido[array_pos]) return 0;
    
    return cantidad_numeros[array_pos];
}

/**
 * Función para verificar si un JSON específico fue recibido
 */
bool json_fue_recibido(int json_id) {
    if (json_id < 1 || json_id > max_jsons) return false;
    return json_recibido[json_id - 1];
}

/**
 * Función para mostrar todos los números de un JSON
 */
void mostrar_numeros_json(int json_id) {
    if (!json_fue_recibido(json_id)) {
        printf("JSON %d no recibido\n", json_id);
        return;
    }
    
    int array_pos = json_id - 1;
    printf("JSON %d contiene %d números: ", json_id, cantidad_numeros[array_pos]);
    
    for (int i = 0; i < cantidad_numeros[array_pos]; i++) {
        printf("%d ", matriz_numeros[array_pos][i]);
    }
    printf("\n");
}

/**
 * Función para mostrar estado general del sistema
 */
void mostrar_estado_sistema() {
    printf("\n=== ESTADO DEL SISTEMA ===\n");
    printf("JSONs recibidos: %d/%d\n", total_jsons_recibidos, max_jsons);
    printf("Listo para imprimir: %s\n", listo_para_imprimir ? "SÍ" : "NO");
    
    if (total_jsons_recibidos < max_jsons) {
        printf("JSONs faltantes: ");
        for (int i = 0; i < max_jsons; i++) {
            if (!json_recibido[i]) {
                printf("%d ", i + 1);
            }
        }
        printf("\n");
    }
    printf("=========================\n\n");
}

/**
 * Función para inicializar el sistema
 */
void inicializar_sistema_json() {
    // Limpiar todas las variables
    for (int i = 0; i < 100; i++) {
        json_recibido[i] = false;
        cantidad_numeros[i] = 0;
        for (int j = 0; j < 50; j++) {
            matriz_numeros[i][j] = 0;
        }
    }
    
    total_jsons_recibidos = 0;
    listo_para_imprimir = false;
    
    printf("Sistema JSON inicializado para %d JSONs\n", max_jsons);
}

/**
 * Función para procesar todos los JSONs recibidos (tu bucle mejorado)
 */
void procesar_todos_los_jsons() {
    if (!listo_para_imprimir) {
        printf("Error: Sistema no está listo para imprimir\n");
        return;
    }
    
    printf("Iniciando procesamiento de todos los JSONs...\n");
    
    for (int i = 0; i < max_jsons; i++) {
        if (!json_recibido[i]) continue;  // Saltar JSONs no recibidos
        
        int json_id = i + 1;
        printf("\n--- Procesando JSON %d ---\n", json_id);
        
        // Procesar cada número del JSON actual
        for (int j = 0; j < cantidad_numeros[i]; j++) {
            int numero_actual = matriz_numeros[i][j];
            
            printf("JSON %d, posición %d: valor=%d\n", json_id, j, numero_actual);
            
            if (numero_actual == 0) {
                printf("Número 0 detectado\n");
                // StepIz(30);  // Tu función
            } else {
                printf("Procesando número: %d\n", numero_actual);
                // StepIz(30);
                // braille(numero_actual);
                // vTaskDelay(pdMS_TO_TICKS(700));
                // mover_actuador();
            }
        }
        // StepDer(840);  // Tu función al final de cada JSON
    }
    
    printf("\n¡Procesamiento completo!\n");
}