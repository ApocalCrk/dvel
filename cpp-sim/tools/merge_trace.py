"""
Merge per-node trace JSON files emitted by sim_sybil into a single artifact with a header.

Input: trace_sybil_node*.json (arrays of rows with node_id, row_index, fields).
Output: merged_trace.json in the same directory by default.
"""

import argparse
import glob
import json
import os
from typing import Any, Dict, List, Optional, Tuple


def load_trace(path: str) -> List[Dict[str, Any]]:
    with open(path, "r") as f:
        return json.load(f)


def merge_traces(paths: List[str]) -> Tuple[List[Dict[str, Any]], Optional[str]]:
    rows: List[Dict[str, Any]] = []
    final_root: Optional[str] = None
    for p in paths:
        data = load_trace(p)
        for row in data:
            rows.append(row)
            mr = row.get("merkle_root")
            if mr:
                final_root = mr
    # Deterministic ordering: timestamp, then node_id, then row_index
    rows.sort(key=lambda r: (r.get("timestamp", 0), r.get("node_id", 0), r.get("row_index", 0)))
    return rows, final_root


def main() -> None:
    parser = argparse.ArgumentParser(description="Merge per-node trace JSON files from sim_sybil.")
    parser.add_argument(
        "--dir",
        default=".",
        help="Directory containing trace_*_node*.json (default: current directory)",
    )
    parser.add_argument(
        "--out",
        default="merged_trace.json",
        help="Output JSON file path (default: merged_trace.json in --dir)",
    )
    parser.add_argument(
        "--prefix",
        default="trace_sybil",
        help="Filename prefix for per-node traces (default: trace_sybil)",
    )
    parser.add_argument(
        "--skew",
        type=int,
        default=1_000_000,
        help="Max backward skew configured in sim (default: 1_000_000)",
    )
    parser.add_argument(
        "--max-pending",
        type=int,
        default=16384,
        help="Pending pool cap (default: 16384)",
    )
    parser.add_argument(
        "--max-drain",
        type=int,
        default=16384,
        help="Pending drain cap (default: 16384)",
    )
    parser.add_argument(
        "--warmup",
        type=int,
        default=4,
        help="Sybil warmup ticks (default: 4)",
    )
    parser.add_argument(
        "--quarantine",
        type=int,
        default=12,
        help="Sybil quarantine ticks (default: 12)",
    )
    parser.add_argument(
        "--fixed-point-scale",
        type=int,
        default=1000,
        help="Sybil fixed point scale (default: 1000)",
    )
    parser.add_argument(
        "--max-link-walk",
        type=int,
        default=4096,
        help="Sybil max link walk (default: 4096)",
    )
    parser.add_argument(
        "--protocol-version",
        type=int,
        default=1,
        help="Protocol version (default: 1)",
    )

    args = parser.parse_args()

    pattern = f"{args.prefix}_node*.json"
    trace_paths = sorted(glob.glob(os.path.join(args.dir, pattern)))
    if not trace_paths:
        raise SystemExit(f"No {pattern} found in {args.dir}")

    rows, final_root = merge_traces(trace_paths)

    header = {
        "protocol_version": args.protocol_version,
        "max_backward_skew": args.skew,
        "max_pending_total": args.max_pending,
        "max_drain_steps": args.max_drain,
        "sybil_config": {
            "warmup_ticks": args.warmup,
            "quarantine_ticks": args.quarantine,
            "fixed_point_scale": args.fixed_point_scale,
            "max_link_walk": args.max_link_walk,
        },
        "final_merkle_root": final_root,
        "sources": [os.path.basename(p) for p in trace_paths],
    }

    out_path = args.out
    if not os.path.isabs(out_path):
        out_path = os.path.join(args.dir, out_path)

    with open(out_path, "w") as f:
        json.dump({"header": header, "rows": rows}, f, separators=(",", ":"))

    print(f"Wrote merged trace with {len(rows)} rows to {out_path}")


if __name__ == "__main__":
    main()
