#!/usr/bin/env bash
set -euo pipefail

root="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
helper="$root/build_scghost.py"
if command -v cygpath >/dev/null 2>&1; then
    helper="$(cygpath -w "$helper")"
fi
for python in python3 python; do
    if command -v "$python" >/dev/null 2>&1 && "$python" -c 'import sys; sys.exit(sys.version_info < (3, 9))' 2>/dev/null; then
        exec "$python" "$helper" build "$@"
    fi
done
printf 'build.sh: Python 3.9 or newer is required.\n' >&2
exit 127
