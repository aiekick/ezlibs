#!/usr/bin/env bash
set -euo pipefail

# Usage: merge_profraw.sh PROFRAW_DIR OUT_PROFDATA
# PROFRAW_DIR must be the directory that *directly* contains the .profraw
# files (no implicit /profraw subpath is appended). The per-subsystem
# EzCoverage macro already passes the fully-qualified per-subsystem path.

PROFRAW_DIR="${1:?usage: merge_profraw.sh PROFRAW_DIR OUT_PROFDATA}"
OUT_PROFDATA="${2:?usage: merge_profraw.sh PROFRAW_DIR OUT_PROFDATA}"

mapfile -d '' FILES < <(find "${PROFRAW_DIR}" -type f -name '*.profraw' -print0 || true)
echo "[merge_profraw] DIR=${PROFRAW_DIR} files=${#FILES[@]}"
if ((${#FILES[@]}==0)); then
  echo "[merge_profraw] Aucun .profraw"
  exit 1
fi

llvm-profdata merge -o "${OUT_PROFDATA}" "${FILES[@]}"
echo "[merge_profraw] OK -> ${OUT_PROFDATA} (${#FILES[@]} fichiers)"
