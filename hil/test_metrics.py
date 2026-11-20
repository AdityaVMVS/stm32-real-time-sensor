import pytest

from hil.metrics import MASK32, Sample, response_metrics


def test_response_and_timer_wrap():
    samples = [
        Sample(i, (MASK32 - 5_000 + i * 10_000) & MASK32, 42,
               2457 if i == 0 else 2048, 500, 400, 0, 0)
        for i in range(52)
    ]
    metrics = response_metrics(samples, baseline=0.0)
    assert metrics["period_jitter_max_us"] == 0
    assert metrics["latency_max_us"] == 42
    assert metrics["overshoot_percent"] == pytest.approx(20)
    assert metrics["settling_ms"] == 10


def test_missing_frame_and_unsettled_response():
    samples = [Sample(1, 0, 30, 1000, 500, 900, 0, 0),
               Sample(3, 20_000, 31, 1100, 500, 900, 0, 0)]
    metrics = response_metrics(samples, baseline=0.0)
    assert metrics["dropped_samples"] == 1
    assert metrics["period_jitter_max_us"] is None
    assert metrics["settling_ms"] is None


@pytest.mark.parametrize("line", ["1,2", "1,2,3,9999,0,0,0,0", "1,2,3,4,0,0,0,x"])
def test_malformed_serial_line(line):
    with pytest.raises((ValueError, TypeError)):
        Sample.parse(line)
