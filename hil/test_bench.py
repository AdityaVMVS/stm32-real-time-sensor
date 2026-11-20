"""Run against a flashed NUCLEO-F767ZI with the README wiring."""

from contextlib import suppress
import csv
import json
import os
from dataclasses import asdict
from pathlib import Path
from statistics import median
from time import monotonic

import pytest
import serial

from hil.metrics import Sample, elapsed_us, response_metrics


def collect(port, count, *, match=lambda sample: True, timeout=10):
    samples = []
    deadline = monotonic() + timeout
    while len(samples) < count and monotonic() < deadline:
        try:
            sample = Sample.parse(port.readline().decode("ascii"))
        except (UnicodeError, ValueError, TypeError):
            continue
        if samples or match(sample):
            samples.append(sample)
    assert len(samples) == count, f"Expected {count} samples, received {len(samples)}"
    return samples


@pytest.mark.skipif(not os.getenv("HIL_PORT"), reason="Set HIL_PORT to run on the wired bench")
def test_step_and_sensor_short():
    capture_dir = Path(os.getenv("HIL_CAPTURE_DIR", "captures"))
    capture_dir.mkdir(parents=True, exist_ok=True)
    phases = []
    with serial.Serial(os.environ["HIL_PORT"], 115200, timeout=0.1, write_timeout=1) as port:
        try:
            port.reset_input_buffer()
            port.write(b"R")
            baseline = collect(port, 301, match=lambda s: s.seq == 1, timeout=5)
            phases.append(("baseline", baseline))
            assert max(s.raw for s in baseline[-20:]) <= 100
            assert all(s.target == s.duty == s.fault == s.clamp == 0 for s in baseline)

            port.write(b"2")
            step = collect(port, 601, match=lambda s: s.target == 650)
            phases.append(("step", step))
            assert all(s.target == 650 and s.fault == 0 for s in step)
            assert all(0 <= s.duty <= 1000 for s in step)
            metrics = response_metrics(step, median(s.raw for s in baseline[-20:]) / 4095)

            port.write(b"F")
            short = collect(port, 75, match=lambda s: s.clamp == 1)
            phases.append(("short", short))
            first_fault = next((s for s in short if s.fault), None)
            if first_fault:
                metrics["fault_trip_ms"] = elapsed_us(short[0].start_us, first_fault.start_us) / 1000
            port.write(b"U")
            released = collect(port, 40, match=lambda s: s.clamp == 0)
            phases.append(("released", released))
            (capture_dir / "metrics.json").write_text(json.dumps(metrics, indent=2) + "\n")

            assert metrics["dropped_samples"] == 0
            assert metrics["latency_max_us"] < 3000
            assert metrics["period_jitter_max_us"] < 1000
            assert metrics["overshoot_percent"] < 20
            assert metrics["settling_ms"] is not None and metrics["settling_ms"] < 5000
            assert first_fault is not None and first_fault.fault == 2
            assert metrics["fault_trip_ms"] <= 300
            assert all(s.fault == 2 and s.duty == 0 for s in short if s.seq >= first_fault.seq)
            assert all(s.fault == 2 and s.duty == 0 for s in released)
        finally:
            # The MCU samples one UART character per cycle. Space the commands.
            with suppress(OSError, AssertionError):
                port.write(b"0")
                collect(port, 1, match=lambda s: s.target == 0, timeout=1)
                port.write(b"U")
            with (capture_dir / "telemetry.csv").open("w", newline="") as file:
                writer = csv.writer(file)
                writer.writerow(["phase", *Sample.__dataclass_fields__])
                for phase, samples in phases:
                    writer.writerows([phase, *asdict(s).values()] for s in samples)
