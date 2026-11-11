#!/usr/bin/env bash
# Host tests for City Radio's plain C parts (no device, no ESP-IDF needed).
set -euo pipefail
cd "$(dirname "$0")"
OUT=$(mktemp -d)
cc -std=c11 -Wall -Wextra -Werror -O1 -o "$OUT/test_radio" test_radio.c ../main/stations.c ../main/spectrum.c ../main/pcm_conv.c -lm
"$OUT/test_radio"
cc -std=c11 -Wall -Wextra -Werror -O1 -o "$OUT/test_selection" test_selection.c ../main/selection.c
"$OUT/test_selection"
rm -rf "$OUT"
