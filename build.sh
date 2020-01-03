#!/bin/bash -e
cd "$(dirname "$0")"

OPT_HELP=false
OPT_CONFIG=false
OPT_G=false
OPT_DEV=false
USAGE_EXIT_CODE=0

# parse args
while [[ $# -gt 0 ]]; do
  case "$1" in
  -h|-help|--help)
    OPT_HELP=true
    shift
    ;;
  -config|--config)
    OPT_CONFIG=true
    shift
    ;;
  -g)
    OPT_G=true
    shift
    ;;
  -dev|--dev)
    OPT_G=true
    OPT_DEV=true
    shift
    ;;
  *)
    echo "$0: Unknown option $1" >&2
    OPT_HELP=true
    USAGE_EXIT_CODE=1
    shift
    ;;
  esac
done
if $OPT_HELP; then
  echo "usage: $0 [options]"
  echo "options:"
  echo "  -h, -help  Show help on stdout."
  echo "  -config    Configure build even if config.sh <> build.ninja is up to date."
  echo "  -g         Build debug build instead of release build."
  echo "  -dev       Build & run debug build. Implies -g"
  exit $USAGE_EXIT_CODE
fi

if $OPT_CONFIG || [ config.sh -nt build.ninja ]; then
  ./config.sh
fi

python3 misc/gen_parselet_map.py

if $OPT_G; then
  ninja debug
  if $OPT_DEV; then
    lldb -bo r ./build/wp.g example/mem.w
    time ./build/wp.g example/mem.w >/dev/null 2>&1
  fi
else
  ninja release
fi


# pushd ../.. >/dev/null
# WASI_SDKROOT=$PWD/wasi-sdk-8.0
# WASI_SYSROOT=$WASI_SDKROOT/share/wasi-sysroot
# wasi_clang=$WASI_SDKROOT/bin/clang
# wasmtime=$PWD/wasmtime/wasmtime
# # export PATH=$WASI_SDKROOT/bin:$PWD/wasmtime:$PATH
# popd >/dev/null

# $wasi_clang --sysroot "$WASI_SYSROOT" wp.c -o wp.wasm
# pushd .. >/dev/null
# $wasmtime --dir=. tool/wp.wasm src/mem.w

# clang -g -DDEBUG tool/util.c tool/sds.c tool/wp.c -o wp
# lldb -bo r ./tool/wp src/mem.w
