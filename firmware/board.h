#ifndef BOARD_H
#define BOARD_H

#include <stdbool.h>
#include <stdint.h>

#define PWM_MAX 1000U

void board_init(void);
uint32_t clock_us(void);
void pwm_write(uint16_t duty);
void adc_start(void);
uint16_t adc_result(void);
int uart_read(void);
void uart_write(const char *line);
void fixture_clamp(bool active);

#endif
