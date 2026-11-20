#include <inttypes.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "stm32f7xx.h"

#include "board.h"
#include "pid.h"

enum {
    PERIOD_MS = 10,
    ADC_MAX = 4095,
    RAIL_LIMIT = 20,
    FAULT_ADC = 1,
    FAULT_LOW = 2,
    FAULT_HIGH = 3,
    FAULT_TIMING = 4,
};

typedef struct {
    uint32_t seq;
    uint32_t start_us;
    uint32_t latency_us;
    uint16_t raw;
    uint16_t target;
    uint16_t duty;
    uint8_t fault;
    bool clamp;
} Frame;

static TaskHandle_t controller;
static QueueHandle_t telemetry;

void ADC_IRQHandler(void)
{
    BaseType_t wake = pdFALSE;
    uint32_t reading = adc_result();
    xTaskNotifyFromISR(controller, reading, eSetValueWithOverwrite, &wake);
    portYIELD_FROM_ISR(wake);
}

void vApplicationStackOverflowHook(TaskHandle_t task, char *name)
{
    (void)task;
    (void)name;
    fail_safe();
}

static void control_task(void *argument)
{
    (void)argument;
    TickType_t next = xTaskGetTickCount();
    Frame frame = {0};
    Pid pid = {0};
    unsigned low_count = 0;
    unsigned high_count = 0;
    bool adc_stalled = false;

    for (;;) {
        xTaskDelayUntil(&next, pdMS_TO_TICKS(PERIOD_MS));
        switch (uart_read()) {
        case '0': frame.target = 0; break;
        case '1': frame.target = 250; break;
        case '2': frame.target = 650; break;
#if HIL
        case 'F':
            frame.clamp = true;
            fixture_clamp(true);
            break;
        case 'U':
            frame.clamp = false;
            fixture_clamp(false);
            break;
        case 'R':
            pwm_write(0);
            NVIC_SystemReset();
            break;
#endif
        default: break;
        }

        uint32_t start = clock_us();
        uint32_t dt_us = frame.seq ? start - frame.start_us : PERIOD_MS * 1000U;
        frame.seq++;
        frame.start_us = start;

        uint32_t reading;
        if (!adc_stalled) {
            adc_start();
            if (xTaskNotifyWait(0, 0, &reading, pdMS_TO_TICKS(3)) == pdPASS) {
                frame.raw = (uint16_t)reading;
            } else {
                adc_stalled = true;
                frame.fault = FAULT_ADC;
            }
        }

        if (!frame.fault) {
            low_count = (frame.raw <= 8 && frame.duty >= 100) ? low_count + 1 : 0;
            high_count = frame.raw >= 4090 ? high_count + 1 : 0;
            if (dt_us < 5000U || dt_us > 15000U) {
                frame.fault = FAULT_TIMING;
            } else if (low_count >= RAIL_LIMIT) {
                frame.fault = FAULT_LOW;
            } else if (high_count >= RAIL_LIMIT) {
                frame.fault = FAULT_HIGH;
            }
        }

        if (frame.fault || !frame.target) {
            pid = (Pid){0};
            frame.duty = 0;
        } else {
            float output = pid_step(&pid, (float)frame.target / PWM_MAX,
                                    (float)frame.raw / ADC_MAX, (float)dt_us / 1000000.0f);
            frame.duty = (uint16_t)(output * PWM_MAX + 0.5f);
        }
        pwm_write(frame.duty);
        frame.latency_us = clock_us() - start;
        xQueueOverwrite(telemetry, &frame);
    }
}

static void telemetry_task(void *argument)
{
    (void)argument;
    Frame frame;
    char line[96];
    for (;;) {
        xQueueReceive(telemetry, &frame, portMAX_DELAY);
        int bytes = snprintf(line, sizeof(line),
                             "%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%u,%u,%u,%u,%u\n",
                             frame.seq, frame.start_us, frame.latency_us,
                             (unsigned)frame.raw, (unsigned)frame.target, (unsigned)frame.duty,
                             (unsigned)frame.fault, (unsigned)frame.clamp);
        if (bytes > 0 && (size_t)bytes < sizeof(line)) {
            uart_write(line);
        }
    }
}

int main(void)
{
    board_init();
    telemetry = xQueueCreate(1, sizeof(Frame));
    configASSERT(telemetry);
    BaseType_t control_created = xTaskCreate(control_task, "control", 256, NULL, 3, &controller);
    BaseType_t uart_created = xTaskCreate(telemetry_task, "telemetry", 640, NULL, 1, NULL);
    configASSERT(control_created == pdPASS && uart_created == pdPASS);
    vTaskStartScheduler();
    fail_safe();
}
