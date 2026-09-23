#!/bin/sh
# The resident name is compiled into the driver. Exec will not open a copy
# installed under a different basename, even when the file itself is valid.
set -eu

if [ "$#" -ne 2 ]; then
    echo "usage: $0 DRIVER_BINARY INSTALL_PATH" >&2
    exit 2
fi

binary=$1
install_path=$2
expected=plipbox.device

if [ ! -f "$binary" ]; then
    echo "missing driver binary: $binary" >&2
    exit 1
fi

if [ "${install_path##*/}" != "$expected" ]; then
    echo "driver must be installed as $expected, not $install_path" >&2
    exit 1
fi

if ! strings -a "$binary" | grep -Fqx "$expected"; then
    echo "driver binary lacks its expected resident name: $expected" >&2
    exit 1
fi
