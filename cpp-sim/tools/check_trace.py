"""
Sanity-check merged_trace.json before feeding it to a prover.

Checks:
- Parent presence flag matches zero-hash vs non-zero prev_hash.
- Quarantine transitions: if ancestor_check is false, quarantined_until_after >= quarantined_until_before + quarantine_ticks.
- Author weight bounds: 0 <= author_weight_fp <= fixed_point_scale; zero if tick < quarantined_until_after.
- Merkle roots: header final_merkle_root matches the last non-null row root.
"""

import argparse
import json
import sys
from typing import Any, Dict


def is_zero_hash(h: str) -> bool:
    return h == "00" * 32


def main() -> None:
    ap = argparse.ArgumentParser(description="Check invariants in merged_trace.json")
    ap.add_argument("trace", help="Path to merged_trace.json")
    args = ap.parse_args()

    with open(args.trace, "r") as f:
        doc = json.load(f)

    header: Dict[str, Any] = doc.get("header", {})
    rows = doc.get("rows", [])
    if not rows:
        sys.exit("no rows in trace")

    q_ticks = header.get("sybil_config", {}).get("quarantine_ticks", 0)
    fp_scale = header.get("sybil_config", {}).get("fixed_point_scale", 0)
    final_root_hdr = header.get("final_merkle_root")

    last_root = None
    errors = 0
    for idx, r in enumerate(rows):
        ts = r.get("timestamp", 0)
        prev = r.get("prev_hash", "")
        parent_present = r.get("parent_present", False)
        ancestor_ok = r.get("ancestor_check", True)
        q_before = r.get("quarantined_until_before", 0)
        q_after = r.get("quarantined_until_after", 0)
        aw = r.get("author_weight_fp", 0)
        mr = r.get("merkle_root")

        if not is_zero_hash(prev) and not parent_present:
            print(f"[row {idx}] non-zero prev_hash but parent_present=false")
            errors += 1

        if not ancestor_ok and q_after < q_before + q_ticks:
            print(f"[row {idx}] quarantine_after too small: before={q_before} after={q_after} q_ticks={q_ticks}")
            errors += 1

        if aw < 0 or aw > fp_scale:
            print(f"[row {idx}] author_weight_fp out of bounds: {aw}")
            errors += 1

        if ts < q_after and aw != 0:
            print(f"[row {idx}] weight not zero during quarantine: ts={ts} q_after={q_after} aw={aw}")
            errors += 1

        if mr:
            last_root = mr

    if final_root_hdr and last_root and final_root_hdr != last_root:
        print(f"[root] header final_merkle_root {final_root_hdr} != last row root {last_root}")
        errors += 1

    if errors == 0:
        print(f"OK: {len(rows)} rows, final_root={last_root}")
    else:
        sys.exit(f"Found {errors} invariant violations")


if __name__ == "__main__":
    main()
