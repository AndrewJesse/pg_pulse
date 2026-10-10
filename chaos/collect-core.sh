#!/usr/bin/env bash
set -euo pipefail

core_file=""
for candidate in core*; do
	[[ -f "$candidate" ]] || continue
	if [[ -z "$core_file" || "$candidate" -nt "$core_file" ]]; then
		core_file=$candidate
	fi
done

if [[ -z "$core_file" ]]; then
	printf 'No core dump found in the current directory\n' >&2
	exit 1
fi

gdb -batch -ex "bt full" "$core_file"
