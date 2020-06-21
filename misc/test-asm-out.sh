#!/bin/bash
PROG=$1

echo "hexdump -> $PROG.hex"
hexdump -v -C "$PROG" | tee "$PROG".hex

echo "./$PROG"

PROGNAME=$(basename "$PROG")
pushd "$(dirname "$PROG")" >/dev/null
PROGDIR=$PWD
popd >/dev/null

docker run --rm -v "$PROGDIR:/mnt1" debian:latest "/mnt1/$PROGNAME"
EXIT_STATUS=$?
echo "Exit status: $EXIT_STATUS"
