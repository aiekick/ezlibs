#!/usr/bin/env bash
set -euo pipefail

# Usage: export_lcov_genhtml.sh BUILD_DIR LCOV_INFO LCOV_HTML_DIR PROFDATA_PATH EXES_FILE [INCLUDE_ONLY...paths]
# PROFDATA_PATH is the merged .profdata file produced by merge_profraw.sh.
# It is now passed explicitly so the script does not have to know the
# per-subsystem naming convention of the EzCoverage macro.

BUILD_DIR="${1:?usage: export_lcov_genhtml.sh BUILD_DIR LCOV_INFO LCOV_HTML_DIR PROFDATA_PATH EXES_FILE [INCLUDE_ONLY...]}"
LCOV_INFO="${2:?usage: export_lcov_genhtml.sh BUILD_DIR LCOV_INFO LCOV_HTML_DIR PROFDATA_PATH EXES_FILE [INCLUDE_ONLY...]}"
LCOV_HTML="${3:?usage: export_lcov_genhtml.sh BUILD_DIR LCOV_INFO LCOV_HTML_DIR PROFDATA_PATH EXES_FILE [INCLUDE_ONLY...]}"
OUT_PROFDATA="${4:?usage: export_lcov_genhtml.sh BUILD_DIR LCOV_INFO LCOV_HTML_DIR PROFDATA_PATH EXES_FILE [INCLUDE_ONLY...]}"
EXES_FILE="${5:?usage: export_lcov_genhtml.sh BUILD_DIR LCOV_INFO LCOV_HTML_DIR PROFDATA_PATH EXES_FILE [INCLUDE_ONLY...]}"
shift 5
INCLUDE_ONLY=( "$@" )  # zéro, un ou plusieurs répertoires à extraire (pattern /* ajouté)

# The test executables come from the list made by list_test_executables.py
mapfile -t EXES < "${EXES_FILE}"
echo "[lcov] executables=${#EXES[@]}"

IGNORE_RE='(^/usr/|^/opt/|.*/(llvm|clang)/|.*/include/c\+\+/|.*/libc\+\+/|.*/third_party/|.*/CMakeFiles/)'

# lcov 2.x checks the llvm-cov branch records strictly (a line hit with no
# branch evaluated is an error there, so is an exclude pattern that matches
# nothing) and renamed the branch option ; lcov 1.x
# knows neither of the new flags, so the options follow the major version.
LCOV_MAJOR=$(lcov --version | grep -oE '[0-9]+' | head -n 1)
if [[ "${LCOV_MAJOR:-1}" -ge 2 ]]; then
  LCOV_IGNORE=(--ignore-errors inconsistent,inconsistent,unsupported,unsupported,unused,unused)
  LCOV_OPTS=(--rc branch_coverage=1 "${LCOV_IGNORE[@]}")
else
  LCOV_IGNORE=()
  LCOV_OPTS=(--rc lcov_branch_coverage=1)
fi

: > "${LCOV_INFO}"
for exe in "${EXES[@]}"; do
  echo "[lcov] export ${exe}"
  llvm-cov export "${exe}" \
    -instr-profile="${OUT_PROFDATA}" \
    -ignore-filename-regex="${IGNORE_RE}" \
    -format=lcov >> "${LCOV_INFO}" 2>/dev/null || true
done

# Filet de sécurité: remove chemins systèmes/outils
EXCLUDE_GLOBS=(
  "/usr/*" "*/lib/*/clang/*" "*/lib/llvm-*/*" "*/include/c++/*"
  "*/llvm/*" "*/clang/*" "*/third_party/*" "*/CMakeFiles/*"
)
lcov --quiet "${LCOV_OPTS[@]}" \
     --remove "${LCOV_INFO}" "${EXCLUDE_GLOBS[@]}" \
     --output-file "${LCOV_INFO}.tmp" || true
if grep -q '^SF:' "${LCOV_INFO}.tmp"; then
  mv "${LCOV_INFO}.tmp" "${LCOV_INFO}"
else
  rm -f "${LCOV_INFO}.tmp"
fi

# Optionnel: extraire uniquement tes sources publiques
if ((${#INCLUDE_ONLY[@]})); then
  PATTERNS=()
  for p in "${INCLUDE_ONLY[@]}"; do
    PATTERNS+=( "${p%/}/*" )
  done
  echo "[lcov] extract only: ${PATTERNS[*]}"
  if lcov --quiet "${LCOV_OPTS[@]}" \
          --extract "${LCOV_INFO}" "${PATTERNS[@]}" \
          --output-file "${LCOV_INFO}.filtered" && \
     grep -q '^SF:' "${LCOV_INFO}.filtered"; then
    mv "${LCOV_INFO}.filtered" "${LCOV_INFO}"
  else
    rm -f "${LCOV_INFO}.filtered"
    echo "[lcov] extract a échoué, on continue avec le tracefile complet (pas filtré)."
  fi
fi

rm -rf "${LCOV_HTML}"
genhtml "${LCOV_INFO}" "${LCOV_IGNORE[@]}" \
  --output-directory "${LCOV_HTML}" \
  --legend --branch-coverage --function-coverage --demangle-cpp \
  --title "Coverage (llvm-cov → lcov → genhtml)"
echo "[lcov] HTML : ${LCOV_HTML}/index.html"
