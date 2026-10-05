#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -lt 3 ]; then
    echo "usage: $0 <adasim> <config> <scenario>" >&2
    exit 2
fi

EXE="$1"
CONFIG="$2"
SCENARIO="$3"

if [ ! -x "$EXE" ]; then
    echo "ADASim not executable: $EXE" >&2
    exit 2
fi

WORK_DIR="$(mktemp -d /tmp/adasim-smoke.XXXXXX)"
cleanup() {
    rm -rf "$WORK_DIR"
}
trap cleanup EXIT

cd "$WORK_DIR"

set +e
"$EXE" --headless --test --config "$CONFIG" --scenario "$SCENARIO"
EXIT_CODE=$?
set -e

if [ "$EXIT_CODE" -ne 0 ]; then
    echo "headless smoke failed with exit code $EXIT_CODE" >&2
    if [ -f test_result/report.txt ]; then
        echo "----- report.txt -----" >&2
        cat test_result/report.txt >&2
    fi
    exit "$EXIT_CODE"
fi

if [ ! -f test_result/report.txt ]; then
    echo "report.txt was not generated" >&2
    exit 2
fi

if ! grep -q '^PASS:' test_result/report.txt; then
    echo "report.txt missing PASS field" >&2
    exit 2
fi

exit 0