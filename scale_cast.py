#!/usr/bin/env python3
"""Scale event timestamps in an asciinema cast file."""

import argparse
import json
import math
from pathlib import Path


def parse_speed(value: str) -> float:
    try:
        speed = float(value)
    except ValueError as error:
        raise argparse.ArgumentTypeError("speed must be a number") from error

    if not math.isfinite(speed) or speed <= 0:
        raise argparse.ArgumentTypeError(
            "speed must be a finite number greater than 0"
        )
    return speed


def scale_cast(input_path: Path, output_path: Path, speed: float) -> None:
    if input_path.resolve() == output_path.resolve():
        raise ValueError("input and output paths must be different")

    has_header = False
    with input_path.open("r", encoding="utf-8", newline="") as source:
        with output_path.open("w", encoding="utf-8", newline="") as output:
            for line_number, line in enumerate(source, start=1):
                if line.endswith("\r\n"):
                    ending = "\r\n"
                elif line.endswith("\n"):
                    ending = "\n"
                else:
                    ending = ""
                content = line[: -len(ending)] if ending else line

                try:
                    record = json.loads(content)
                except json.JSONDecodeError as error:
                    raise ValueError(
                        f"{input_path}:{line_number}: invalid JSON: {error.msg}"
                    ) from error

                if not has_header:
                    if not isinstance(record, dict):
                        raise ValueError(
                            f"{input_path}:{line_number}: expected a JSON object "
                            "in the cast header"
                        )
                    output.write(line)
                    has_header = True
                    continue

                if (
                    not isinstance(record, list)
                    or not record
                    or isinstance(record[0], bool)
                    or not isinstance(record[0], (int, float))
                ):
                    raise ValueError(
                        f"{input_path}:{line_number}: expected an event with "
                        "a numeric timestamp"
                    )

                try:
                    timestamp = record[0] * speed
                except OverflowError as error:
                    raise ValueError(
                        f"{input_path}:{line_number}: scaled timestamp is not finite"
                    ) from error

                if not math.isfinite(timestamp):
                    raise ValueError(
                        f"{input_path}:{line_number}: scaled timestamp is not finite"
                    )

                record[0] = timestamp
                output.write(json.dumps(record, separators=(",", ":")) + ending)

    if not has_header:
        raise ValueError(f"{input_path}: cast file is empty")


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Multiply event timestamps in an asciinema .cast file."
    )
    parser.add_argument("input", type=Path, help="input .cast file")
    parser.add_argument(
        "output",
        nargs="?",
        type=Path,
        help="output file (default: <input>_scaled.cast)",
    )
    parser.add_argument(
        "--speed",
        type=parse_speed,
        default=1.0,
        metavar="FACTOR",
        help="timestamp multiplier; 1.0 leaves timing unchanged (default: 1.0)",
    )
    args = parser.parse_args()
    output_path = args.output or args.input.with_name(
        f"{args.input.stem}_scaled{args.input.suffix}"
    )

    try:
        scale_cast(args.input, output_path, args.speed)
    except (OSError, ValueError) as error:
        parser.error(str(error))

    print(f"Wrote {output_path}")


if __name__ == "__main__":
    main()
