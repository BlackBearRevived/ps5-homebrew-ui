#!/usr/bin/env bash
# ps5-homebrew-ui - Package the staged showcase app with the boilerplate's pinned kit.
# Copyright (C) 2026 BlackBearReloaded
# SPDX-License-Identifier: GPL-3.0-or-later

set -euo pipefail
cd -- "$(dirname -- "$0")/.."
title=$(python3 -c 'import json; print(json.load(open("sce_sys/param.json"))["titleId"])')
[[ $title =~ ^PPSA[0-9]{5}$ ]]
test -s "dist/$title.zip"
kit=".deps/ps5-pkgkit-0.1.0"
python3 -m venv "$kit"
"$kit/bin/python" -m pip install --no-index --no-deps --require-hashes \
    'https://github.com/BlackBearRevived/ps5-pkgkit/releases/download/v0.1.0/ps5_pkgkit-0.1.0-py3-none-any.whl#sha256=71cfa598c509322c6752c094486935a8334d7cf0fceb73e838810e46a3823541'
"$kit/bin/ps5-pkgkit" build "dist/$title" -o "dist/$title.pkg"
"$kit/bin/ps5-pkgkit" verify "dist/$title.pkg"
(cd dist && sha256sum "$title.pkg" > "$title.pkg.sha256" && sha256sum "$title.zip" > SHA256SUMS)
