#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <pca9685.h>
#include <string.h>
#include <esp_log.h>
#include "driver/uart.h"
#include "driver/ledc.h"
#include "freertos/timers.h"
#include "freertos/queue.h"
#include "Mylibraries.h"
#include "cJSON.h"
#include "libreriajson.h"
static QueueHandle_t uart2_queue; // Cola para eventos de UART2
#define UART2 UART_NUM_2
static char buffer_acumulado[256] = {0};
static int buffer_index = 0;
int servo_numero = 0;
int servo_grados = 0;
// posiciones braille
#define BRAILLE_000 0
#define BRAILLE_001 1
#define BRAILLE_010 2
#define BRAILLE_011 3
#define BRAILLE_100 4
#define BRAILLE_101 5
#define BRAILLE_110 6
#define BRAILLE_111 7
bool binario[6];
// Funciónes para servo
void move_servo_from_message(const char *message)
{
    char *message_copy = strdup(message); // Copia para no modificar el original

    char *token = strtok(message_copy, ",");
    if (token == NULL)
    {
        printf("Error: Formato de mensaje inválido\n");
        free(message_copy);
        return;
    }

    int servo_num = atoi(token); // Convertir N a entero

    token = strtok(NULL, ",");
    if (token == NULL)
    {
        printf("Error: Grados no especificados\n");
        free(message_copy);
        return;
    }

    int grados = atoi(token); // Convertir grados a entero

    // Mover el servo especificado
    servo_numero = servo_num;
    servo_grados = grados;

    free(message_copy);
}

// funciones del uart
static void init_uart(uart_port_t uart_num, int tx_pin, int rx_pin, QueueHandle_t *queue)
{
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };

    uart_param_config(uart_num, &uart_config);
    uart_set_pin(uart_num, tx_pin, rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(uart_num, BUF_SIZE, BUF_SIZE, 10, queue, 0);

    printf("UART%d initialized on TX pin %d, RX pin %d\n", uart_num, tx_pin, rx_pin);
}
// Tarea para manejar eventos UART0
static void uart2_event_task(void *pvParameters)
{
    uart_event_t event;
    uint8_t *data = (uint8_t *)malloc(BUF_SIZE);

    while (true)
    {
        // Espera de eventos en la cola UART
        if (xQueueReceive(uart2_queue, (void *)&event, portMAX_DELAY))
        {
            switch (event.type)
            {
            case UART_DATA: // Datos recibidos
                // Leer los datos del buffer UART
                int len = uart_read_bytes(UART2, data, event.size, portMAX_DELAY);
                data[len] = '\0'; // Asegurar que los datos son una cadena

                // Procesar cada carácter recibido
                for (int i = 0; i < len; i++)
                {
                    if (data[i] == '|')
                    {

                        // Encontramos el delimitador, procesar el mensaje completo
                        buffer_acumulado[buffer_index] = '\0'; // Terminar la cadena
                        printf("Mensaje completo recibido: %s\n", buffer_acumulado);
                        // Aquí puedes procesar el mensaje completo
                        // verificar_y_procesar_uart(buffer_acumulado);
                        // Limpiar el buffer para el próximo mensaje
                        buffer_index = 0;
                        memset(buffer_acumulado, 0, sizeof(buffer_acumulado));
                    }
                    else
                    {
                        // Acumular el carácter si hay espacio en el buffer
                        if (buffer_index < sizeof(buffer_acumulado) - 1)
                        {
                            buffer_acumulado[buffer_index] = data[i];
                            buffer_index++;
                        }
                        else
                        {
                            printf("Error: Buffer lleno, reiniciando\n");
                            buffer_index = 0;
                            memset(buffer_acumulado, 0, sizeof(buffer_acumulado));
                        }
                    }
                }
                break;

            case UART_FIFO_OVF: // Desbordamiento de FIFO
                printf("¡Desbordamiento de FIFO!\n");
                uart_flush_input(UART2);
                xQueueReset(uart2_queue);
                break;

            case UART_BUFFER_FULL: // Buffer lleno
                printf("¡Buffer lleno!\n");
                uart_flush_input(UART2);
                xQueueReset(uart2_queue);
                break;

            case UART_BREAK:
                printf("UART Break detectado\n");
                break;

            case UART_PARITY_ERR:
                printf("Error de paridad\n");
                break;

            case UART_FRAME_ERR:
                printf("Error de frame\n");
                break;

            default:
                printf("Evento UART no manejado: %d\n", event.type);
                break;
            }
        }
    }

    free(data);
    vTaskDelete(NULL);
}

void uart_printf(uart_port_t uart_num, const char *fmt, ...)
{
    char buffer[UART_TX_BUFFER_SIZE];
    va_list args;

    // Formatear la cadena usando va_list
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args); // Crear el mensaje formateado
    va_end(args);

    // Enviar la cadena formateada por UART
    uart_write_bytes(uart_num, buffer, strlen(buffer));
}
// funcion principal en bucle

void servo_pos(int num, int pos)
{

    switch (pos)
    {
    case 0: //
        // Mover servo 1 a 0 grados
        move_servo(num, 0);
        break;
    case 1:
        // Mover servo 1 a 90 grados
        move_servo(num, 22.5);
        break;
    case 2:
        // Mover servo 1 a 180 grados
        move_servo(num, 45);
        break;
    case 3:
        // Mover servo 1 a 180 grados
        move_servo(num, 67.5);
        break;
    case 4:
        // Mover servo 1 a 180 grados
        move_servo(num, 90);
        break;
    case 5:
        // Mover servo 1 a 180 grados
        move_servo(num, 112.5);
        break;
    case 6:
        // Mover servo 1 a 180 grados
        move_servo(num, 135);
        break;
    case 7:
        // Mover servo 1 a 180 grados
        move_servo(num, 157.5);
        break;
    default:
        printf("Posición inválida para servo 1\n");
        break;
    }
}
void decimal_to_binary_array(int decimal)
{
    // Limpiar el array primero
    for (int i = 0; i < 6; i++)
    {
        binario[i] = false;
    }

    // Si el número es 0, ya está listo (todo false)
    if (decimal == 0)
    {
        return;
    }

    // Convertir a binario empezando desde el bit menos significativo (índice 5)
    for (int i = 5; i >= 0 && decimal > 0; i--)
    {
        binario[i] = (decimal % 2) == 1;
        decimal /= 2;
    }
}

void separar_bits_to_decimal(int *primeros_3, int *siguientes_3)
{
    // Convertir los primeros 3 bits (índices 0, 1, 2) a decimal
    *primeros_3 = 0;
    for (int i = 0; i < 3; i++)
    {
        if (binario[i])
        {
            *primeros_3 += (1 << (2 - i)); // 2^(2-i)
        }
    }

    // Convertir los siguientes 3 bits (índices 3, 4, 5) a decimal
    *siguientes_3 = 0;
    for (int i = 3; i < 6; i++)
    {
        if (binario[i])
        {
            *siguientes_3 += (1 << (5 - i)); // 2^(5-i)
        }
    }
}
void braille(int caracter)
{
    int grupo1, grupo2;
    decimal_to_binary_array(caracter);
    separar_bits_to_decimal(&grupo1, &grupo2);
    // Aquí puedes usar el array binario para representar el carácter en Braille
    servo_pos(0, grupo1);
    printf("Braille grupo 1: %d,grupo2: %d \n", grupo1, grupo2);
    servo_pos(1, grupo2);
    printf("\n");
}
void fin_de_carrera_task(void *param)
{
    while (true)
    {
        // Leer estado del pin 2
        int nivel_pin_2 = gpio_get_level(PIN_FIN_CARRERA_1);
        fin_carrera_1_activo = (nivel_pin_2 == 0) ? true : false;

        // Leer estado del pin 0
        int nivel_pin_0 = gpio_get_level(PIN_FIN_CARRERA_2);
        fin_carrera_2_activo = (nivel_pin_0 == 0) ? true : false;

        // Leer estado del pin 14
        int nivel_pin_papel = gpio_get_level(PIN_PAPEL);
        papel_activo = (nivel_pin_papel == 0) ? true : false;

        // Leer estado del pin 16
        int nivel_pin_movimiento = gpio_get_level(PIN_MOVIMIENTO);
        movimiento_activo = (nivel_pin_movimiento == 0) ? true : false;

        if (!iniciando)
        {
            if (!fin_carrera_1_activo || !fin_carrera_2_activo)
            {
                gpio_set_level(RESET, 0);
            }
        }
        // Delay de 50ms entre lecturas
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
void app_main()
{
    // Init i2cdev library
    // ESP_ERROR_CHECK(pca9658_init());
    // configurar_gpio();
    // init_gpio_output();
    // set_pwm();
    init_uart(UART2, 16, 17, &uart2_queue); // UART0 TX: GPIO01, RX: GPIO3
    // Crear tareas para manejar eventos UART
    xTaskCreate(fin_de_carrera_task, "fin_carrera_task", 2048, NULL, 5, NULL);
    xTaskCreate(uart2_event_task, "uart2_event_task", 2048, NULL, 11, NULL);
    inicializar_sistema_json();
    // init_pap();
    while (true)
    {
        printf("esperando mensajes\n");
        uart_printf(UART2, "mensaje desde uart2\n|");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    while (false)
    {
        printf("Estado de fin de carrera 1: %s\n", fin_carrera_1_activo ? "Activo" : "Inactivo");
        printf("Estado de fin de carrera 2: %s\n", fin_carrera_2_activo ? "Activo" : "Inactivo");
        printf("Estado de fin de papel: %s\n", papel_activo ? "Activo" : "Inactivo");
        printf("Estado de fin de movimiento: %s\n", movimiento_activo ? "Activo" : "Inactivo");

        if (listo_para_imprimir)
        {
            tomar_hoja();
            vTaskDelay(pdMS_TO_TICKS(500));
            while (papel_activo)
            {
                tomar_hoja();
                vTaskDelay(pdMS_TO_TICKS(1000));
            }

            for (int i = 0; i < max_jsons; i++)
            {

                for (int j = 0; j < 28; j++)
                {
                    printf("Procesando JSON %d, caracter %d: valor=%d, char='%c'\n",
                           i + 1, j + 1, (int)matriz_numeros[i][j], matriz_numeros[i][j]);

                    // Verificar si llegamos al final de la cadena

                    if (matriz_numeros[i][j] == '0' || matriz_numeros[i][j] == '\0')
                    {
                        printf("Carácter '0' detectado\n");
                        StepIz(40);
                    }
                    else
                    {
                        printf("Procesando carácter: %c\n", matriz_numeros[i][j]);
                        braille(matriz_numeros[i][j]);
                        StepIz(40);
                        // vTaskDelay(pdMS_TO_TICKS(700));
                        mover_actuador();
                    }
                }

                fin_linea();
                mover_linea();
            }
        }
        mostrar_estado_sistema();
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
