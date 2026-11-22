#!/bin/sh
# Usage: tests/run_tests.sh [path-to-sudoku_validator]
# Feeds every tests/cases/*.in into the console validator and checks that the
# output contains every line of the matching tests/cases/*.expected file.
BIN="${1:-./sudoku_validator}"
DIR="$(cd "$(dirname "$0")" && pwd)/cases"
pass=0; fail=0

for in_file in "$DIR"/*.in; do
    name=$(basename "$in_file" .in)
    exp_file="$DIR/$name.expected"
    out=$(timeout 10 "$BIN" < "$in_file" 2>&1)
    code=$?
    ok=1
    while IFS= read -r line || [ -n "$line" ]; do
        [ -z "$line" ] && continue
        echo "$out" | grep -qF -- "$line" || { ok=0; echo "  missing: $line"; }
    done < "$exp_file"
    [ "$code" -eq 124 ] && { ok=0; echo "  timeout"; }
    if [ $ok -eq 1 ]; then echo "PASS $name"; pass=$((pass+1))
    else echo "FAIL $name"; fail=$((fail+1)); fi
done

echo "passed=$pass failed=$fail"
[ $fail -eq 0 ]
