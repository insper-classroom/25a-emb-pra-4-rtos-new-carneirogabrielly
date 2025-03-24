/*
 * LED blink with FreeRTOS
 */
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>

#include "ssd1306.h"
#include "gfx.h"

#include "pico/stdlib.h"
#include <stdio.h>

const uint BTN_1_OLED = 28;
const uint BTN_2_OLED = 26;
const uint BTN_3_OLED = 27;

const uint LED_1_OLED = 20;
const uint LED_2_OLED = 21;
const uint LED_3_OLED = 22;

const int PIN_ECHO = 16;
const int PIN_TRIGGER = 17;
const int T_TRIGGER = 10;

QueueHandle_t xQueueTime;
SemaphoreHandle_t xSemaphoreTrigger;
QueueHandle_t xQueueDistance;

void pin_callback(uint gpio, uint32_t events)
{
    if (events & GPIO_IRQ_EDGE_RISE || events & GPIO_IRQ_EDGE_FALL)
    {
        uint64_t time = to_us_since_boot(get_absolute_time());
        xQueueSendFromISR(xQueueTime, &time, 0);
    }
}

void trigger_task(void *p)
{
    gpio_init(PIN_TRIGGER);
    gpio_set_dir(PIN_TRIGGER, GPIO_OUT);

    int delay = 100;
    while (true)
    {
        gpio_put(PIN_TRIGGER, 1);
        vTaskDelay(pdMS_TO_TICKS(1));
        gpio_put(PIN_TRIGGER, 0);
        xSemaphoreGive(xSemaphoreTrigger);
        vTaskDelay(pdMS_TO_TICKS(delay));
    }
}

void echo_task(void *p)
{
    gpio_init(PIN_ECHO);
    gpio_set_dir(PIN_ECHO, GPIO_IN);
    gpio_pull_up(PIN_ECHO);
    gpio_set_irq_enabled_with_callback(PIN_ECHO, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &pin_callback);

    uint64_t start_time;
    uint64_t end_time;
    uint64_t time;
    bool flag_start_time = true;

    while (true)
    {
        if (xQueueReceive(xQueueTime, &time, pdMS_TO_TICKS(100)) == pdTRUE)
        {
            if (flag_start_time)
            {
                start_time = time;
                flag_start_time = false;
            }
            else
            {
                end_time = time;
                uint32_t duracao_us = end_time - start_time;
                float distancia = (duracao_us / 2.0) * 0.0343;
                xQueueSend(xQueueDistance, &distancia, pdMS_TO_TICKS(10));
                flag_start_time = true;
            }
        }
    }
}

void oled_task(void *p)
{
    printf("Inicializando Driver\n");
    ssd1306_init();

    printf("Inicializando GLX\n");
    ssd1306_t disp;
    gfx_init(&disp, 128, 32);

    printf("Inicializando btn and LEDs\n");
    oled1_btn_led_init();

    char barra = 0;
    float distancia;

    while (1)
    {
        
        if ((xSemaphoreTake(xSemaphoreTrigger, pdMS_TO_TICKS(1000)) == pdTRUE) && ((xQueueReceive(xQueueDistance, &distancia, pdMS_TO_TICKS(1000)))))
        {
            char info[20];
            if (distancia <= 450 && distancia >= 2)
            {
                barra = distancia * 128 / 450;
                sprintf(info, "%.2f cm", distancia);
            }
            else
            {
                sprintf(info, "Falha na leitura");
            }
            gfx_clear_buffer(&disp);
            gfx_draw_string(&disp, 0, 0, 1, info);
            gfx_draw_line(&disp, 0, 27, barra, 27);
            gfx_show(&disp);
        }
    }
}

void oled1_btn_led_init(void)
{
    gpio_init(LED_1_OLED);
    gpio_set_dir(LED_1_OLED, GPIO_OUT);

    gpio_init(LED_2_OLED);
    gpio_set_dir(LED_2_OLED, GPIO_OUT);

    gpio_init(LED_3_OLED);
    gpio_set_dir(LED_3_OLED, GPIO_OUT);

    gpio_init(BTN_1_OLED);
    gpio_set_dir(BTN_1_OLED, GPIO_IN);
    gpio_pull_up(BTN_1_OLED);

    gpio_init(BTN_2_OLED);
    gpio_set_dir(BTN_2_OLED, GPIO_IN);
    gpio_pull_up(BTN_2_OLED);

    gpio_init(BTN_3_OLED);
    gpio_set_dir(BTN_3_OLED, GPIO_IN);
    gpio_pull_up(BTN_3_OLED);
}

int main()
{
    stdio_init_all();

    xSemaphoreTrigger = xSemaphoreCreateBinary();

    if (xSemaphoreTrigger == NULL)
    {
        printf("Falha em criar o semaforo\n");
    }

    xQueueTime = xQueueCreate(32, sizeof(uint64_t));

    if (xQueueTime == NULL)
    {
        printf("Falha em criar a fila Time\n");
    }

    xQueueDistance = xQueueCreate(32, sizeof(float));

    if (xQueueDistance == NULL)
    {
        printf("Falha em criar a fila Distance\n");
    }

    // xTaskCreate(oled1_demo_2, "Demo 2", 4095, NULL, 1, NULL);
    // xTaskCreate(oled1_demo_1, "Demo 1", 4095, NULL, 1, NULL);
    xTaskCreate(trigger_task, "Trigger", 4095, NULL, 1, NULL);
    xTaskCreate(echo_task, "Echo", 4095, NULL, 1, NULL);
    xTaskCreate(oled_task, "Oled", 4095, NULL, 1, NULL);

    vTaskStartScheduler();

    while (true)
        ;
}