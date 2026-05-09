#!/usr/bin/env python3

import argparse
import csv
from dataclasses import dataclass
from datetime import datetime, timedelta, timezone
from pathlib import Path
import random


DEFAULT_SEED = 7
DEFAULT_OUTPUT_DIR = Path("benchmark_data")
PROFILES = {
    "10k_3": {"events": 10_000, "symbols": 3},
    "1m_3": {"events": 1_000_000, "symbols": 3},
    "1m_30": {"events": 1_000_000, "symbols": 30},
    "1m_300": {"events": 1_000_000, "symbols": 300},
}


@dataclass(frozen=True)
class Quote:
    bid_price: float
    bid_size: float
    ask_price: float
    ask_size: float


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Generate deterministic quote-row replay CSVs for arb_benchmark."
    )
    parser.add_argument("--profile", choices=sorted(PROFILES.keys()))
    parser.add_argument("--events", type=int, help="Total number of quote events to emit.")
    parser.add_argument("--symbols", type=int, help="Total number of symbols to emit.")
    parser.add_argument("--seed", type=int, default=DEFAULT_SEED)
    parser.add_argument("--output", type=Path, help="Output CSV path.")
    return parser.parse_args()


def resolve_generation_config(args: argparse.Namespace) -> tuple[int, int, Path]:
    profile = PROFILES.get(args.profile, {})
    event_count = args.events if args.events is not None else profile.get("events")
    symbol_count = args.symbols if args.symbols is not None else profile.get("symbols")
    if event_count is None or symbol_count is None:
        raise ValueError("Specify --profile or provide both --events and --symbols.")
    if event_count <= 0 or symbol_count < 3:
        raise ValueError("events must be positive and symbols must be at least 3.")

    if args.output is not None:
        output_path = args.output
    elif args.profile is not None:
        output_path = DEFAULT_OUTPUT_DIR / f"generated_{args.profile}.csv"
    else:
        output_path = DEFAULT_OUTPUT_DIR / f"generated_{event_count}_{symbol_count}.csv"
    return event_count, symbol_count, output_path


def build_symbol_list(symbol_count: int) -> list[str]:
    symbols = ["BTC-USD", "ETH-USD", "ETH-BTC"]
    for index in range(symbol_count - 3):
        symbols.append(f"ASSET{index + 1:03d}-USD")
    return symbols


def format_timestamp(event_index: int) -> str:
    timestamp = datetime(2026, 1, 1, tzinfo=timezone.utc) + timedelta(milliseconds=event_index)
    return timestamp.isoformat().replace("+00:00", "Z")


def triangle_quote(symbol: str, round_index: int) -> Quote:
    if symbol == "BTC-USD":
        center = 50_005.0 + ((round_index % 17) - 8) * 0.75
        return Quote(
            bid_price=round(center - 5.0, 6),
            bid_size=1.0 + (round_index % 5) * 0.25,
            ask_price=round(center + 5.0, 6),
            ask_size=1.2 + (round_index % 7) * 0.2,
        )

    if symbol == "ETH-USD":
        center = 2_500.5 + ((round_index * 3 % 19) - 9) * 0.15
        return Quote(
            bid_price=round(center - 0.6, 6),
            bid_size=8.0 + (round_index % 9),
            ask_price=round(center + 0.6, 6),
            ask_size=8.5 + (round_index % 11),
        )

    edge_center = 0.05105 + ((round_index * 5 % 13) - 6) * 0.00001
    return Quote(
        bid_price=round(edge_center - 0.00004, 8),
        bid_size=4.0 + (round_index % 6) * 0.5,
        ask_price=round(edge_center + 0.00004, 8),
        ask_size=4.5 + (round_index % 8) * 0.5,
    )


def background_quote(symbol_index: int, round_index: int, rng: random.Random) -> Quote:
    base_price = 25.0 + symbol_index * 3.5
    drift = ((round_index * ((symbol_index % 7) + 1)) % 23) - 11
    jitter = rng.uniform(-0.05, 0.05)
    center = base_price + drift * 0.07 + jitter
    spread = 0.04 + (symbol_index % 5) * 0.01
    bid_size = 50.0 + (round_index % 13) * 2.0 + symbol_index % 11
    ask_size = 55.0 + (round_index % 17) * 2.0 + symbol_index % 13
    return Quote(
        bid_price=round(center - spread / 2.0, 6),
        bid_size=round(bid_size, 6),
        ask_price=round(center + spread / 2.0, 6),
        ask_size=round(ask_size, 6),
    )


def quote_for_symbol(symbol: str, symbol_index: int, round_index: int, rng: random.Random) -> Quote:
    if symbol in {"BTC-USD", "ETH-USD", "ETH-BTC"}:
        return triangle_quote(symbol, round_index)
    return background_quote(symbol_index, round_index, rng)


def generate_replay(event_count: int, symbol_count: int, seed: int, output_path: Path) -> None:
    rng = random.Random(seed)
    symbols = build_symbol_list(symbol_count)
    output_path.parent.mkdir(parents=True, exist_ok=True)

    with output_path.open("w", newline="") as handle:
        writer = csv.writer(handle)
        writer.writerow(["timestamp", "symbol", "bid_price", "bid_size", "ask_price", "ask_size"])

        for event_index in range(event_count):
            symbol_index = event_index % len(symbols)
            round_index = event_index // len(symbols)
            symbol = symbols[symbol_index]
            quote = quote_for_symbol(symbol, symbol_index, round_index, rng)
            writer.writerow(
                [
                    format_timestamp(event_index),
                    symbol,
                    quote.bid_price,
                    quote.bid_size,
                    quote.ask_price,
                    quote.ask_size,
                ]
            )


def main() -> int:
    args = parse_args()
    try:
        event_count, symbol_count, output_path = resolve_generation_config(args)
    except ValueError as error:
        print(f"error: {error}")
        return 1

    generate_replay(event_count, symbol_count, args.seed, output_path)
    print(
        f"generated replay: path={output_path} events={event_count} symbols={symbol_count} seed={args.seed}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
