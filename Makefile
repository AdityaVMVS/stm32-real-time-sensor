CROSS ?= arm-none-eabi-
CC := $(CROSS)gcc
OBJCOPY := $(CROSS)objcopy
DEPS ?= deps
FREERTOS := $(DEPS)/FreeRTOS-Kernel
DEVICE := $(DEPS)/cmsis_device_f7
CMSIS := $(DEPS)/CMSIS_5/CMSIS/Core/Include
PORT := $(FREERTOS)/portable/GCC/ARM_CM7/r0p1
HIL ?= 0
BUILD := build/f767zi/hil-$(HIL)
ARCH := -mcpu=cortex-m7 -mthumb -mfpu=fpv5-d16 -mfloat-abi=hard
CPPFLAGS := -DSTM32F767xx -DHIL=$(HIL) -Ifirmware -I$(DEVICE)/Include -I$(CMSIS) -I$(FREERTOS)/include -I$(PORT)
CFLAGS := $(ARCH) -std=c11 -Os -g -Wall -Wextra -ffunction-sections -fdata-sections
KERNEL := tasks.c queue.c list.c portable/GCC/ARM_CM7/r0p1/port.c portable/MemMang/heap_4.c
SOURCES := $(wildcard firmware/*.c) $(addprefix $(FREERTOS)/,$(KERNEL)) $(DEVICE)/Source/Templates/gcc/startup_stm32f767xx.s

.PHONY: all deps clean flash
all: $(BUILD)/controller.bin

deps:
	test -d $(FREERTOS) || git clone --depth 1 --branch V11.1.0 https://github.com/FreeRTOS/FreeRTOS-Kernel.git $(FREERTOS)
	test -d $(DEVICE) || git clone --depth 1 --branch v1.2.10 https://github.com/STMicroelectronics/cmsis_device_f7.git $(DEVICE)
	test -d $(DEPS)/CMSIS_5 || git clone --depth 1 --branch 5.9.0 https://github.com/ARM-software/CMSIS_5.git $(DEPS)/CMSIS_5

$(BUILD)/controller.elf: $(SOURCES) $(wildcard firmware/*.h) firmware/stm32f767zi.ld Makefile
	mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SOURCES) -nostartfiles --specs=nosys.specs -T firmware/stm32f767zi.ld -Wl,--gc-sections,-Map,$(BUILD)/controller.map -o $@

$(BUILD)/controller.bin: $(BUILD)/controller.elf
	$(OBJCOPY) -O binary $< $@

flash: $(BUILD)/controller.bin
	st-flash --reset write $< 0x08000000

clean:
	rm -rf build
