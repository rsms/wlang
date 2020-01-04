#!/bin/bash -e
cd "$(dirname "$0")"
autorun src/*.c src/*.h config.sh example/*.w -- sh -c 'killall wp.g ; ./build.sh -dev'
