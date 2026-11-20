"""Read device telemetry and calculate response metrics from its clock."""

from dataclasses import dataclass
from statistics import median

PERIOD_US = 10_000
MASK32 = 2**32 - 1


@dataclass(frozen=True)
class Sample:
    seq: int
    start_us: int
    latency_us: int
    raw: int
    target: int
    duty: int
    fault: int
    clamp: int

    @classmethod
    def parse(cls, line: str) -> "Sample":
        sample = cls(*map(int, line.strip().split(",")))
        limits = (MASK32, MASK32, MASK32, 4095, 1000, 1000, 4, 1)
        if any(not 0 <= value <= limit for value, limit in zip(vars(sample).values(), limits)):
            raise ValueError("Telemetry field outside its expected range")
        return sample


def elapsed_us(start: int, end: int) -> int:
    return (end - start) & MASK32


def response_metrics(samples: list[Sample], baseline: float) -> dict:
    """Use a 5% band around the target, scaled to the step amplitude."""
    if len(samples) < 2:
        raise ValueError("At least two step samples are needed")
    target = samples[0].target / 1000
    step = target - baseline
    if step <= 0 or any(s.target != samples[0].target for s in samples):
        raise ValueError("Expected a positive step and one constant target")

    feedback = [s.raw / 4095 for s in samples]
    overshoot = 100 * max(0.0, (max(feedback) - target) / step)
    last_outside = max(
        (i for i, value in enumerate(feedback) if abs(value - target) > 0.05 * step),
        default=-1,
    )
    settled_at = last_outside + 1
    settling_ms = None
    if settled_at < len(samples):
        stable_us = elapsed_us(samples[settled_at].start_us, samples[-1].start_us)
        if stable_us >= 500_000:
            settling_ms = elapsed_us(samples[0].start_us, samples[settled_at].start_us) / 1000

    intervals = [
        elapsed_us(prev.start_us, cur.start_us) for prev, cur in zip(samples, samples[1:])
    ]
    dropped = sum(
        elapsed_us(prev.seq, cur.seq) - 1 for prev, cur in zip(samples, samples[1:])
    )
    return {
        "latency_median_us": median(s.latency_us for s in samples),
        "latency_max_us": max(s.latency_us for s in samples),
        "period_jitter_max_us": max(abs(i - PERIOD_US) for i in intervals) if dropped == 0 else None,
        "dropped_samples": dropped,
        "overshoot_percent": overshoot,
        "settling_ms": settling_ms,
    }
