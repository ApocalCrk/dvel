#!/usr/bin/env bash
set -euo pipefail

# Poll a shared folder for *.manifest files and reassemble verified files.
# Env:
#   SHARE_DIR      - folder containing chunks + manifests (default: ~/dvel-share)
#   DVEL_FILE_BIN  - path to dvel-file binary (default: dvel-file in PATH)
#   POLL_SECONDS   - delay between scans (default: 5)

SHARE_DIR="${SHARE_DIR:-$HOME/dvel-share}"
DVEL_FILE_BIN="${DVEL_FILE_BIN:-dvel-file}"
POLL_SECONDS="${POLL_SECONDS:-5}"

mkdir -p "$SHARE_DIR"

while true; do
  processed_any=false
  for manifest in "$SHARE_DIR"/*.manifest; do
    [ -e "$manifest" ] || continue
    processed_any=true
    base="$(basename "$manifest" .manifest)"
    output="$SHARE_DIR/$base.out"
    echo "[worker] processing $manifest"
    if "$DVEL_FILE_BIN" download "$manifest" "$SHARE_DIR" "$output"; then
      echo "[worker] wrote $output"
    else
      echo "[worker] failed to process $manifest" >&2
    fi
  done
  if [ "$processed_any" = false ]; then
    sleep "$POLL_SECONDS"
  fi
done
