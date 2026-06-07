"""Shared utilities for benchmark normalizers."""

import json
import os
import platform
import re
import subprocess
from datetime import datetime, timezone
from pathlib import Path

SCHEMA_VERSION = 1


def _repo_root() -> Path:
    """Find the repo root via git, falling back to relative heuristic."""
    try:
        root = subprocess.check_output(
            ["git", "rev-parse", "--show-toplevel"],
            stderr=subprocess.DEVNULL,
            text=True,
        ).strip()
        return Path(root)
    except (subprocess.CalledProcessError, FileNotFoundError):
        # Fallback: assume this file is at tools/normalizers/common.py
        return Path(__file__).resolve().parent.parent.parent


RESULTS_DIR = _repo_root() / "benchmarks" / "results"


def get_git_commit() -> str:
    try:
        return subprocess.check_output(
            ["git", "rev-parse", "--short", "HEAD"],
            stderr=subprocess.DEVNULL,
            text=True,
        ).strip()
    except (subprocess.CalledProcessError, FileNotFoundError):
        return "unknown"


def get_git_branch() -> str:
    try:
        return subprocess.check_output(
            ["git", "rev-parse", "--abbrev-ref", "HEAD"],
            stderr=subprocess.DEVNULL,
            text=True,
        ).strip()
    except (subprocess.CalledProcessError, FileNotFoundError):
        return "unknown"


def get_cpu_model() -> str:
    system = platform.system()
    if system == "Linux":
        try:
            with open("/proc/cpuinfo") as f:
                for line in f:
                    if line.startswith("model name"):
                        return line.split(":", 1)[1].strip()
        except OSError:
            pass
    elif system == "Darwin":
        try:
            return subprocess.check_output(
                ["sysctl", "-n", "machdep.cpu.brand_string"],
                stderr=subprocess.DEVNULL,
                text=True,
            ).strip()
        except (subprocess.CalledProcessError, FileNotFoundError):
            pass
    return "unknown"


def get_cpu_cores() -> int:
    try:
        return os.cpu_count() or 0
    except Exception:
        return 0


def get_kernel() -> str:
    try:
        return platform.release()
    except Exception:
        return ""


def get_machine_info() -> dict:
    return {
        "hostname": platform.node(),
        "cpu_model": get_cpu_model(),
        "cpu_cores": get_cpu_cores(),
        "os": f"{platform.system()} {platform.machine()}",
        "kernel": get_kernel(),
    }


def make_envelope(
    tool: str,
    config: dict,
    metrics: dict,
    tags: list[str] | None = None,
    timestamp: datetime | None = None,
) -> dict:
    """Build a canonical benchmark result envelope."""
    if timestamp is None:
        timestamp = datetime.now(timezone.utc)
    return {
        "schema_version": SCHEMA_VERSION,
        "tool": tool,
        "commit": get_git_commit(),
        "branch": get_git_branch(),
        "timestamp": timestamp.isoformat(),
        "machine": get_machine_info(),
        "config": config,
        "metrics": metrics,
        "tags": tags or [],
    }


def metric(value: float | int, unit: str) -> dict:
    """Build a single metric entry."""
    return {"value": value, "unit": unit}


def default_output_path(tool: str, timestamp: datetime | None = None) -> Path:
    """Generate the default output filename."""
    if timestamp is None:
        timestamp = datetime.now(timezone.utc)
    commit = get_git_commit()
    time_str = timestamp.strftime("%Y%m%d_%H%M%S")
    RESULTS_DIR.mkdir(parents=True, exist_ok=True)
    return RESULTS_DIR / f"{commit}_{tool}_{time_str}.json"


def write_result(result: dict, output_path: Path) -> None:
    """Write a canonical result to disk."""
    output_path.parent.mkdir(parents=True, exist_ok=True)
    with output_path.open("w") as f:
        json.dump(result, f, indent=2)
        f.write("\n")
    print(f"wrote: {output_path}")
