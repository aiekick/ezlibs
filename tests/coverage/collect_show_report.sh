#!/usr/bin/env bash
set -euo pipefail

# Usage: collect_show_report.sh BUILD_DIR OUT_DIR PROFDATA_PATH PROFRAW_DIR EXES_FILE
# PROFDATA_PATH is the merged .profdata file produced by merge_profraw.sh.
# PROFRAW_DIR is the directory that *directly* contains the .profraw files
# (used only for reporting the file count). Both are now passed explicitly
# so the script does not have to know the per-subsystem naming convention
# of the EzCoverage macro.

BUILD_DIR="${1:?usage: collect_show_report.sh BUILD_DIR OUT_DIR PROFDATA_PATH PROFRAW_DIR EXES_FILE}"
OUT_DIR="${2:?usage: collect_show_report.sh BUILD_DIR OUT_DIR PROFDATA_PATH PROFRAW_DIR EXES_FILE}"
OUT_PROFDATA="${3:?usage: collect_show_report.sh BUILD_DIR OUT_DIR PROFDATA_PATH PROFRAW_DIR EXES_FILE}"
PROFRAW_DIR="${4:?usage: collect_show_report.sh BUILD_DIR OUT_DIR PROFDATA_PATH PROFRAW_DIR EXES_FILE}"
EXES_FILE="${5:?usage: collect_show_report.sh BUILD_DIR OUT_DIR PROFDATA_PATH PROFRAW_DIR EXES_FILE}"

COV_TXT="${OUT_DIR}/coverage.txt"
RPT_TXT="${OUT_DIR}/report.txt"

# The test executables come from the list made by list_test_executables.py
mapfile -t EXES < "${EXES_FILE}"
echo "[show] executables=${#EXES[@]}"

# OBJ mapping
OBJ_ARGS=$(
  { find "${BUILD_DIR}" -type f -name '*.so*' -print0
    find "${BUILD_DIR}" -type f -name '*.a'    -print0
    find "${BUILD_DIR}" -type f -name '*.o'    -print0; } |
  xargs -0 -I{} printf ' -object %q' {}
)

# Filtre agressif (LLVM/Clang/STL/3rdparty/ CMakeFiles)
IGNORE_RE='(^/usr/|^/opt/|.*/(llvm|clang)/|.*/include/c\+\+/|.*/libc\+\+/|.*/third_party/|.*/CMakeFiles/)'

: > "${COV_TXT}"
for exe in "${EXES[@]}"; do
  [[ "$exe" == *"/CMakeFiles/"* ]] && continue
  echo "===== \"$exe\" =====" >> "${COV_TXT}"
  llvm-cov show "$exe" -instr-profile="${OUT_PROFDATA}" \
    -ignore-filename-regex="${IGNORE_RE}" ${OBJ_ARGS} \
    >> "${COV_TXT}" 2>> "${COV_TXT}" || true
done

: > "${RPT_TXT}"
echo "# profraw files: $(find "${PROFRAW_DIR}" -type f -name '*.profraw' | wc -l)" >> "${RPT_TXT}"
for exe in "${EXES[@]}"; do
  [[ "$exe" == *"/CMakeFiles/"* ]] && continue
  echo "" >> "${RPT_TXT}"
  echo "===== \"$exe\" =====" >> "${RPT_TXT}"
  llvm-cov report "$exe" -instr-profile="${OUT_PROFDATA}" \
    -ignore-filename-regex="${IGNORE_RE}" ${OBJ_ARGS} \
    >> "${RPT_TXT}" 2>> "${RPT_TXT}" || true
done
