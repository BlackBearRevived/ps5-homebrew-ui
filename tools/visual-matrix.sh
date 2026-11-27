#!/usr/bin/env bash
# ps5-homebrew-ui - Render focused, disabled, and modal states for visual review.
# Copyright (C) 2026 BlackBearReloaded
# SPDX-License-Identifier: GPL-3.0-or-later

set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
output=${1:-"$root/build/visual-matrix"}
export HUI_CAPTURE_FILTER=matrix-
for mode in normal reduced-motion; do
    if [[ $mode == reduced-motion ]]; then
        export HUI_REDUCED_MOTION=1
    else
        unset HUI_REDUCED_MOTION
    fi
    bash "$root/tools/host-snapshots.sh" "$output/$mode" components 1280 720
    for theme in acrylic pixel sketch friendly contrast; do
        for state in focused disabled modal; do
            files=("$output/$mode/"??-components-matrix-"$theme-$state".png)
            [[ -s ${files[0]} ]] || {
                echo "Missing $mode screenshot for $theme ($state)" >&2
                exit 1
            }
        done
    done
done
