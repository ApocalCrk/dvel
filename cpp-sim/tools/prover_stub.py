"""
Example prover stub: consume merged_trace.json, perform lightweight checks, and emit a deterministic trace commitment.

This is NOT a real proof; it is a helper to:
- Parse the merged trace
- Re-run basic invariants (final root consistency)
- Produce a SHA256 commitment over header + rows as a stand-in for a PCS commitment
"""

import argparse
import hashlib
import json
import sys
from typing import Any, Dict, List


def load_trace(path: str) -> Dict[str, Any]:
    with open(path, "r") as f:
        return json.load(f)


def check_final_root(doc: Dict[str, Any]) -> None:
    header = doc.get("header", {})
    rows: List[Dict[str, Any]] = doc.get("rows", [])
    if not rows:
        raise SystemExit("no rows in trace")
    final_hdr = header.get("final_merkle_root")
    last_root = None
    for r in rows:
        mr = r.get("merkle_root")
        if mr:
            last_root = mr
    if final_hdr and last_root and final_hdr != last_root:
        raise SystemExit(f"final root mismatch: header={final_hdr} last_row={last_root}")


def trace_commitment(doc: Dict[str, Any]) -> str:
    """
    Deterministic SHA256 over header + rows fields.
    This is a placeholder commitment for external proof systems.
    """
    h = hashlib.sha256()

    def upd(s: str) -> None:
        h.update(s.encode("utf-8"))

    header = doc.get("header", {})
    upd(json.dumps(header, sort_keys=True, separators=(",", ":")))

    for r in doc.get("rows", []):
        # Serialize a stable subset of fields
        parts = [
            r.get("node_id", 0),
            r.get("row_index", 0),
            r.get("prev_hash", ""),
            r.get("author", ""),
            r.get("timestamp", 0),
            r.get("payload_hash", ""),
            r.get("signature", ""),
            int(r.get("parent_present", False)),
            int(r.get("ancestor_check", False)),
            r.get("quarantined_until_before", 0),
            r.get("quarantined_until_after", 0),
            r.get("merkle_root", ""),
            int(r.get("merkle_root_has", False)),
            r.get("preferred_tip", ""),
            int(r.get("preferred_tip_has", False)),
            r.get("author_weight_fp", 0),
        ]
        upd("|".join(str(x) for x in parts))

    return h.hexdigest()


def main() -> None:
    ap = argparse.ArgumentParser(description="Prover stub: commit to merged_trace.json")
    ap.add_argument("trace", help="Path to merged_trace.json")
    args = ap.parse_args()

    doc = load_trace(args.trace)
    check_final_root(doc)
    commit = trace_commitment(doc)
    print(f"trace_commitment_sha256={commit}")


if __name__ == "__main__":
    main()
