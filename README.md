# STM32 Real-Time Sensor & Actuator Control Platform

This project follows a simple feedback loop. An STM32 reads a voltage, compares it with a setpoint, and updates a PWM output. I used an RC circuit as the load so the entire loop can be tested on a desk with a Nucleo board, two resistors, and a capacitor.

The firmware targets the **NUCLEO-F401RE** and uses the STM32 peripheral registers through CMSIS. FreeRTOS handles task scheduling. A Python/pytest bench test drives the board over USB and saves the raw data so the timing and response can be checked later.

## How it works

1. A high-priority FreeRTOS task wakes every 10 ms and starts an ADC1 conversion on PA0. The ADC interrupt sends the reading back to that task using a direct task notification.
2. The PID controller smooths the measurement with a first-order low-pass filter, then compares it with the setpoint. The derivative uses the measured value to avoid a kick at a setpoint change. The output is limited to 0–100% and the integral pauses when it would push the output farther into a limit.
3. TIM3 generates 2 kHz PWM on PA6. A compare-register update takes effect on the next PWM cycle. TIM2 runs at 1 MHz and provides timestamps for the controller and test harness.
4. A one-slot FreeRTOS queue sends the latest reading to a lower-priority USART2 telemetry task. This keeps a slow serial write out of the control loop. A jump in the sequence number shows when the host missed a frame.
5. A conversion timeout or a control period outside 5–15 ms latches a fault. Twenty consecutive high-rail readings also trip the controller. Low-rail readings count toward a fault while the previous PWM command is at least 10%, so a discharged capacitor at idle is allowed. A fault sets duty to zero until the board resets.

## Wiring the bench

Use the Nucleo's ST-LINK USB port for power, flashing, and the virtual serial port. Wire the RC load with the board powered off. The output is for the RC test load. A real motor or other power actuator would need an external driver and its own tuning.

```text
PA6 (D12, PWM) ── 10 kΩ ──●── 1 kΩ ──●── PA0 (A0, ADC)
                          │           │
                        47 µF         └── PB0 (A3, HIL clamp)
                          │
                         GND
```

Connect the capacitor's negative lead to GND if it is polarized. The time constant is about 0.47 s, so the ADC sees the average PWM voltage change gradually. PB0 releases the ADC node normally. In the HIL build, a serial command pulls PB0 low through its open-drain output to simulate a sensor wire shorted to ground. The 1 kΩ resistor limits the capacitor discharge current during this test.

## Build and flash

Install Git, GNU Make, the Arm GNU Toolchain for `arm-none-eabi` including newlib, and `st-flash` from the ST-LINK tools. The build uses the 16 MHz internal reset clock and fetches the pinned FreeRTOS Kernel and CMSIS releases.

```sh
make deps
make HIL=1
make HIL=1 flash
```

Use `make` and `make flash` for the normal build with the fault injector and serial reset command disabled. You can also flash the generated binary at `0x08000000` with STM32CubeProgrammer or debug the ELF through ST-LINK. The board powers up at zero duty with a zero setpoint.

Open the virtual serial port at **115200 baud, 8N1**. Send one ASCII command at a time without a line ending. The firmware checks one byte per control cycle.

| Command | Behavior |
| --- | --- |
| `0` | Setpoint 0%, PWM off |
| `1` / `2` | Setpoint 25% / 65% of ADC full scale |
| `F` / `U` | Short the ADC node to ground / release it (HIL build) |
| `R` | Reset the MCU and clear any latched fault (HIL build) |

Each telemetry line is `seq,start_us,latency_us,raw,target,duty,fault,clamp`. `raw` is in ADC counts (0–4095), while target and duty are in parts per thousand. Fault codes 0–4 mean okay, ADC timeout, low rail, high rail, and timing fault. The clamp flag reports the commanded fixture state.

## Run the tests

```sh
python3 -m venv .venv
. .venv/bin/activate
python -m pip install -r requirements.txt
python -m pytest -q
```

The default run checks the Python measurement code. With the fixture wired and the HIL binary flashed, close any open serial terminal and run the board test. Change the port to match your USB serial device. Use a port such as `COM5` on Windows.

```sh
HIL_PORT=/dev/ttyACM0 python -m pytest -q hil/test_bench.py
```

The test resets the board, records the baseline, steps to 65%, and waits for the RC load to respond. It then shorts the ADC input and checks that the controller latches the low-rail fault, writes zero duty, and stays off when the short is released. Look in `captures/` for the CSV trace and JSON metrics.

| Measurement | How it is calculated |
| --- | --- |
| Control-loop latency | Time from immediately before starting ADC sampling to the PWM compare write. The pin can take up to one more 500 µs PWM period to reflect the change. |
| Scheduling jitter | Largest absolute difference between consecutive sample-start intervals and the 10 ms period. A dropped frame invalidates this calculation. |
| Overshoot | Peak voltage above the setpoint, as a percentage of the commanded step from the measured baseline. |
| Settling time | First time the raw measurement enters a band of ±5% of the step amplitude and stays there through the capture, with at least 500 ms in the band. |
| Fault trip time | Elapsed device time from the first frame with the clamp on to the first latched fault. |

These timings use the STM32 clock, so USB packet arrival times do not affect them. The test verifies the PWM command through telemetry and the circuit response through the actual ADC pin. Checking exact PWM edge timing requires a scope or logic analyzer.

The bench limits in the test are acceptance targets. Physical measurements are pending.

## Source layout

- `firmware/board.c` configures the ADC, PWM, UART, microsecond timer, and optional HIL clamp.
- `firmware/main.c` contains the FreeRTOS tasks, notifications, telemetry queue, and fault logic.
- `firmware/pid.c` contains the filter and PID math.
- `hil/test_bench.py` runs the wired test and saves captures. `hil/metrics.py` calculates the response and timing metrics.
