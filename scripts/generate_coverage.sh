#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR_INPUT="${1:-$(pwd)}"
BUILD_DIR="$(cd "${BUILD_DIR_INPUT}" && pwd)"
SOURCE_DIR_INPUT="${2:-$(pwd)}"
SOURCE_DIR="$(cd "${SOURCE_DIR_INPUT}" && pwd)"
GCOV_BIN="$(command -v gcov || true)"

if [[ -z "${GCOV_BIN}" ]]; then
  echo "gcov is not available on PATH."
  exit 1
fi

OBJ_DIR="${BUILD_DIR}/CMakeFiles/order_book_lib.dir/src"
if [[ ! -d "${OBJ_DIR}" ]]; then
  echo "Expected object directory not found: ${OBJ_DIR}"
  exit 1
fi

mkdir -p "${BUILD_DIR}/coverage"
pushd "${BUILD_DIR}/coverage" >/dev/null

echo "Generating gcov reports from: ${OBJ_DIR}"
rm -f ./*.gcov
: > coverage_raw.txt
: > coverage_summary.txt

GCDA_FILES=()
while IFS= read -r gcda_path; do
  GCDA_FILES+=("${gcda_path}")
done < <(find "${OBJ_DIR}" -maxdepth 1 -name '*.gcda' | sort)

if [[ ${#GCDA_FILES[@]} -eq 0 ]]; then
  echo "No .gcda files found under ${OBJ_DIR}. Run tests first." | tee -a coverage_summary.txt
  exit 1
fi

for gcda_file in "${GCDA_FILES[@]}"; do
  "${GCOV_BIN}" -b -c "${gcda_file}" >> coverage_raw.txt
done

for gcov_report in *.gcov; do
  source_path="$(sed -n "1s/^ *-: *0:Source://p" "${gcov_report}")"
  if [[ "${source_path}" != "${SOURCE_DIR}/src/"* ]]; then
    rm -f "${gcov_report}"
  fi
done

echo "Source coverage summary for: ${SOURCE_DIR}/src" | tee -a coverage_summary.txt
echo | tee -a coverage_summary.txt

if ! ls *.gcov >/dev/null 2>&1; then
  echo "No source .gcov reports found. Check that project source files live under ${SOURCE_DIR}/src." | tee -a coverage_summary.txt
  exit 1
fi

for gcov_report in *.gcov; do
  source_path="$(sed -n "1s/^ *-: *0:Source://p" "${gcov_report}")"
  total=0
  covered=0

  while IFS=: read -r count _; do
    count="${count//[[:space:]]/}"
    if [[ "${count}" =~ ^([0-9]+|#####)$ ]]; then
      ((++total))
      if [[ "${count}" != "#####" ]]; then
        ((++covered))
      fi
    fi
  done < "${gcov_report}"

  if [[ "${total}" -gt 0 ]]; then
    percent="$(awk -v covered="${covered}" -v total="${total}" 'BEGIN { printf "%.2f", covered * 100 / total }')"
  else
    percent="100.00"
  fi

  printf "%s: %s%% lines (%d/%d)\n" "${source_path}" "${percent}" "${covered}" "${total}" | tee -a coverage_summary.txt
done

total_lines=0
covered_lines=0
for gcov_report in *.gcov; do
  while IFS=: read -r count _; do
    count="${count//[[:space:]]/}"
    if [[ "${count}" =~ ^([0-9]+|#####)$ ]]; then
      ((++total_lines))
      if [[ "${count}" != "#####" ]]; then
        ((++covered_lines))
      fi
    fi
  done < "${gcov_report}"
done

overall_percent="$(awk -v covered="${covered_lines}" -v total="${total_lines}" 'BEGIN { if (total == 0) print "100.00"; else printf "%.2f", covered * 100 / total }')"
echo | tee -a coverage_summary.txt
printf "Overall source coverage: %s%% lines (%d/%d)\n" "${overall_percent}" "${covered_lines}" "${total_lines}" | tee -a coverage_summary.txt

echo "Coverage artifacts written to: ${BUILD_DIR}/coverage"
ls -1 *.gcov 2>/dev/null || true

popd >/dev/null
