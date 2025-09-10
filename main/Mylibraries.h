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
// define PCA9685 address
#define ADDR PCA9685_ADDR_BASE
#ifndef APP_CPU_NUM
#define APP_CPU_NUM PRO_CPU_NUM
#endif
#define CONFIG_PWM_FREQ_HZ 50
static const char *TAG = "IMPRESORA";
TimerHandle_t xTimers;
int interval = 100;
int timerId = 1;
int duty_motor1 = 50;
int duty_motor2 = 50;
// Definimos los GPIO del ESP32 para las bobinas
#define STEP GPIO_NUM_12
#define DIR GPIO_NUM_13
#define RESET GPIO_NUM_5
#define time_step 10
// defines del H-Bridge
int step_delay_us;
#define GPIO_output_IN1 GPIO_NUM_27
#define GPIO_output_IN2 GPIO_NUM_14
// defines del uart
#define BUF_SIZE 1024
#define UART_TX_BUFFER_SIZE 1024
#define BUFFER_SIZE 20000
i2c_dev_t dev; // Variable global para el PCA9685
// actuador
#define PIN_ACTUADOR GPIO_NUM_15
// final de carrera
//  Definir pines
#define PIN_FIN_CARRERA_1 GPIO_NUM_2
#define PIN_FIN_CARRERA_2 GPIO_NUM_18
#define PIN_PAPEL GPIO_NUM_16
#define PIN_MOVIMIENTO GPIO_NUM_35
bool fin_carrera_1_activo = false; // true = LOW, false = HIGH
bool fin_carrera_2_activo = false; // true = LOW, false = HIGH
bool papel_activo = false;         // true = LOW, false = HIGH
bool movimiento_activo = false;    // true = LOW, false = HIGH
bool iniciando = false;
void inicializar_gpio_fin_carrera()
{
    // Configurar GPIO 2 como input
    gpio_config_t io_conf_2 = {
        .pin_bit_mask = (1ULL << PIN_FIN_CARRERA_1),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE, // Habilitar pull-up interno
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE};
    gpio_config(&io_conf_2);

    // Configurar GPIO 0 como input
    gpio_config_t io_conf_0 = {
        .pin_bit_mask = (1ULL << PIN_FIN_CARRERA_2),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE, // Habilitar pull-up interno
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE};
    gpio_config(&io_conf_0);
    // Configurar GPIO 14 como input
    gpio_config_t io_conf_14 = {
        .pin_bit_mask = (1ULL << PIN_PAPEL),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE};
    gpio_config(&io_conf_14);

    // Configurar GPIO 16 como input
    gpio_config_t io_conf_16 = {
        .pin_bit_mask = (1ULL << PIN_MOVIMIENTO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE};
    gpio_config(&io_conf_16);
    printf("GPIO fin de carrera inicializados: Pin 2 y Pin 0\n");
}
void configurar_gpio()
{
    gpio_config_t io_conf = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << STEP) | (1ULL << DIR) | (1ULL << PIN_ACTUADOR) | (1ULL << RESET),
        .pull_down_en = 0,
        .pull_up_en = 0,
        .intr_type = GPIO_INTR_DISABLE};
    gpio_config(&io_conf);
    gpio_set_level(DIR, 0);
    gpio_set_level(STEP, 0);
}
void StepIz(int steps)
{
    gpio_set_level(RESET, 1);
    gpio_set_level(DIR, 0); // Dirección hacia la izquierda
    for (int i = 0; i < steps; i++)
    {
        gpio_set_level(STEP, 1);
        vTaskDelay(pdMS_TO_TICKS(time_step)); // Espera 20 ms
        gpio_set_level(STEP, 0);
        vTaskDelay(pdMS_TO_TICKS(time_step)); // Espera 20 ms
    }
    if (!iniciando)
    {
        gpio_set_level(RESET, 0);
    }
}
void StepDer(int steps)
{
    gpio_set_level(RESET, 1);
    gpio_set_level(DIR, 1); // Dirección hacia la derecha
    for (int i = 0; i < steps; i++)
    {
        gpio_set_level(STEP, 1);
        vTaskDelay(pdMS_TO_TICKS(time_step)); // Espera 20 ms
        gpio_set_level(STEP, 0);
        vTaskDelay(pdMS_TO_TICKS(time_step)); // Espera 20 ms
    }
    if (!iniciando)
    {
        gpio_set_level(RESET, 0);
    }
}
// funciones Pca9685

long map(long x, long in_min, long in_max, long out_min, long out_max)
{
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// funciones para el H-Bridge
esp_err_t init_gpio_output()
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << GPIO_output_IN1) | (1ULL << GPIO_output_IN2),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE};

    gpio_config(&io_conf);

    return ESP_OK;
}
// funciones para el pap
esp_err_t set_pwm()
{

    ledc_channel_config_t chanelconfigB = {0};
    chanelconfigB.gpio_num = 25;
    chanelconfigB.speed_mode = LEDC_HIGH_SPEED_MODE;
    chanelconfigB.channel = LEDC_CHANNEL_0;
    chanelconfigB.intr_type = LEDC_INTR_DISABLE;
    chanelconfigB.timer_sel = LEDC_TIMER_0;
    chanelconfigB.duty = 0;
    ledc_channel_config(&chanelconfigB);

    ledc_channel_config_t chanelconfigP = {0};
    chanelconfigP.gpio_num = 26;
    chanelconfigP.speed_mode = LEDC_HIGH_SPEED_MODE;
    chanelconfigP.channel = LEDC_CHANNEL_1;
    chanelconfigP.intr_type = LEDC_INTR_DISABLE;
    chanelconfigP.timer_sel = LEDC_TIMER_0;
    chanelconfigP.duty = 0;

    ledc_channel_config(&chanelconfigP);

    ledc_timer_config_t timerconfigB = {0};
    timerconfigB.speed_mode = LEDC_HIGH_SPEED_MODE;
    timerconfigB.duty_resolution = LEDC_TIMER_10_BIT;
    timerconfigB.timer_num = LEDC_TIMER_0;
    timerconfigB.freq_hz = 2000;

    ledc_timer_config(&timerconfigB);
    return ESP_OK;
}
esp_err_t set_pwm_duty()
{
    ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, duty_motor2);
    ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_1, duty_motor1);
    ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0);
    ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_1);
    return ESP_OK;
}
void Motor_dir_h(int dir, int pwm)
{
    /**
     *dir tiene 3 posiciones: 1 para girar a la derecha, 0 para girar a la izquierda y 2 para detener el motor
     *pwm es el valor del ciclo de trabajo del PWM, que controla la velocidad del motor
     */
    if (dir == 1)
    {
        gpio_set_level(GPIO_output_IN1, 1);
        gpio_set_level(GPIO_output_IN2, 0);
    }
    else if (dir == 0)
    {
        gpio_set_level(GPIO_output_IN1, 0);
        gpio_set_level(GPIO_output_IN2, 1);
    }
    else if (dir == 2)
    {
        gpio_set_level(GPIO_output_IN1, 0);
        gpio_set_level(GPIO_output_IN2, 0);
    }
    duty_motor1 = pwm;
    duty_motor2 = pwm;
    set_pwm_duty();
}
esp_err_t move_servo(int chanel, int grados)
{
    // printf("funcionmoveservo\n");
    //  NO crear dev local - usar el global que ya está inicializado
    int final = map(grados, 0, 180, 143, 471);
    esp_err_t ret = pca9685_set_pwm_value(&dev, chanel, final);
    return ret;
}

esp_err_t pca9658_init()
{
    ESP_ERROR_CHECK(i2cdev_init());

    memset(&dev, 0, sizeof(i2c_dev_t));

    ESP_ERROR_CHECK(pca9685_init_desc(&dev, ADDR, 0, 21, 22));
    ESP_ERROR_CHECK(pca9685_init(&dev));

    ESP_ERROR_CHECK(pca9685_restart(&dev));

    uint16_t freq;
    ESP_ERROR_CHECK(pca9685_set_pwm_frequency(&dev, CONFIG_PWM_FREQ_HZ));
    ESP_ERROR_CHECK(pca9685_get_pwm_frequency(&dev, &freq));

    ESP_LOGI(TAG, "Freq %dHz, real %d", CONFIG_PWM_FREQ_HZ, freq);
    return ESP_OK;
}
void mover_actuador()
{
    gpio_set_level(RESET, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(PIN_ACTUADOR, 1);
    vTaskDelay(pdMS_TO_TICKS(700)); // Esperar 500 ms
    gpio_set_level(PIN_ACTUADOR, 0);
}
void init_pap()
{
    iniciando = true;
    do
    {

        if (!fin_carrera_1_activo)
        {
            break;
        }
        else
        {
            printf("buscando posicion inicial pap...\n");
            StepDer(10);
        }
        printf("Estado de fin de carrera 1: %s\n", fin_carrera_1_activo ? "Activo" : "Inactivo");
    } while (true);

    printf("Posicion inicial pap encontrada\n");
    mover_actuador();
    vTaskDelay(pdMS_TO_TICKS(500));
    mover_actuador();
    StepIz(210);
    gpio_set_level(RESET, 0);
    iniciando = false;
}

void fin_linea()
{
    iniciando = true;
    do
    {

        if (!fin_carrera_1_activo)
        {
            break;
        }
        else
        {
            printf("buscando posicion inicial pap...\n");
            StepDer(10);
        }
        printf("Estado de fin de carrera 1: %s\n", fin_carrera_1_activo ? "Activo" : "Inactivo");
    } while (true);

    printf("Posicion inicial pap encontrada\n");
    StepIz(190);
    gpio_set_level(RESET, 0);
    iniciando = false;
}

void tomar_hoja()
{
    duty_motor1 = 900;
    duty_motor2 = 50;
    // expulsar la hoja actual
    Motor_dir_h(1, 800);
    vTaskDelay(pdMS_TO_TICKS(700)); // 3 segundos
    Motor_dir_h(2, 0);
    vTaskDelay(pdMS_TO_TICKS(500));
    Motor_dir_h(0, 900);
    // Detener motor (ambos en 0)
    vTaskDelay(pdMS_TO_TICKS(50));
    Motor_dir_h(2, 0);
    vTaskDelay(pdMS_TO_TICKS(500));
    // Mover a la derecha (IN1 = 0, IN2 = 1)
    duty_motor1 = 1023;
    Motor_dir_h(1, 1023);
    vTaskDelay(pdMS_TO_TICKS(170)); // 3 segundos
    Motor_dir_h(2, 0);
}

void mover_linea()
{
    Motor_dir_h(1, 800);
    vTaskDelay(pdMS_TO_TICKS(50));
    Motor_dir_h(2, 0);
}