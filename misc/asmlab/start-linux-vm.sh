#!/bin/bash -e
cd "$(dirname "$0")"
echo "Running docker:rsms/emsdk in $PWD"
docker run --rm -it -v "$PWD:/src" rsms/emsdk
