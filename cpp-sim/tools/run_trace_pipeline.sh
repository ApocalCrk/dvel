set -euo pipefail

# Run sim_sybil, merge per-node traces, and sanity-check the merged trace.
# Assumes:
# - rust-core built at rust-core/target/release/libdvel_core.a
# - sim_sybil built at cpp-sim/build/sim_sybil
# - merge_trace.py and check_trace.py in this directory

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT/build"
SIM="$BUILD_DIR/sim_sybil"

if [[ ! -x "$SIM" ]]; then
  echo "sim_sybil not found at $SIM; build it first." >&2
  exit 1
fi

pushd "$BUILD_DIR" >/dev/null
echo "[run] sim_sybil"
"$SIM"
echo "[merge] merge_trace.py"
python3 "$ROOT/tools/merge_trace.py" --dir "$BUILD_DIR" --out merged_trace.json
echo "[check] check_trace.py"
python3 "$ROOT/tools/check_trace.py" "$BUILD_DIR/merged_trace.json"
echo "[commit] prover_stub.py"
commit_line=$(python3 "$ROOT/tools/prover_stub.py" "$BUILD_DIR/merged_trace.json")
echo "$commit_line" | tee "$BUILD_DIR/trace_commitment.txt"
popd >/dev/null

echo "Trace pipeline complete."
echo "Outputs:"
echo "  - $BUILD_DIR/merged_trace.json"
echo "  - $BUILD_DIR/trace_commitment.txt"
