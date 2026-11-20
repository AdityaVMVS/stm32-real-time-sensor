#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include <stdint.h>

void fail_safe(void);

#define configUSE_PREEMPTION                1
#define configUSE_IDLE_HOOK                 0
#define configUSE_TICK_HOOK                 0
#define configCPU_CLOCK_HZ                  16000000UL
#define configTICK_RATE_HZ                  1000U
#define configMAX_PRIORITIES                4
#define configMINIMAL_STACK_SIZE            128
#define configTOTAL_HEAP_SIZE               (12U * 1024U)
#define configMAX_TASK_NAME_LEN             12
#define configUSE_16_BIT_TICKS              0
#define configUSE_MUTEXES                   0
#define configUSE_TIMERS                    0
#define configCHECK_FOR_STACK_OVERFLOW      2
#define configPRIO_BITS                     4
#define configKERNEL_INTERRUPT_PRIORITY     (15U << (8U - configPRIO_BITS))
#define configMAX_SYSCALL_INTERRUPT_PRIORITY (5U << (8U - configPRIO_BITS))
#define INCLUDE_xTaskDelayUntil             1
#define configASSERT(condition)             do { if (!(condition)) fail_safe(); } while (0)

#define vPortSVCHandler     SVC_Handler
#define xPortPendSVHandler  PendSV_Handler
#define xPortSysTickHandler SysTick_Handler

#endif
