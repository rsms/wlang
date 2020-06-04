#!/bin/bash -e
cd "$(dirname "$0")"

OPT_HELP=false
OPT_CONFIG=false
OPT_G=false
OPT_CLEAN=false
OPT_ANALYZE=false
OPT_QUIET=false
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
  -clean|--clean)
    OPT_CLEAN=true
    shift
    ;;
  -a|-analyze|--analyze)
    OPT_ANALYZE=true
    shift
    ;;
  -quiet|--quiet)
    OPT_QUIET=true
    shift
    ;;
  -g)
    OPT_G=true
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
  echo "  -h, -help     Show help on stdout."
  echo "  -config       Configure build even if config.sh <> build.ninja is up to date."
  echo "  -clean        Clean ./build directory before building."
  echo "  -g            Build debug build instead of release build."
  echo "  -a, -analyze  Run static analyzer (Infer https://fbinfer.com) on sources."
  echo "  -quiet        Only print errors."
  exit $USAGE_EXIT_CODE
fi

if $OPT_CLEAN; then
  rm -rf build
fi

if $OPT_CONFIG || [ config.sh -nt build.ninja ] || [ build.in.ninja -nt build.ninja ]; then
  ./config.sh
fi

python3 misc/gen_parselet_map.py &  # patches src/parse.c
python3 misc/gen_ops.py # patches src/ir/op.{h,c}, src/token.h, src/types.h
wait

if $OPT_ANALYZE; then
  if ! (which infer >/dev/null); then
    echo "'infer' not found in PATH. See https://fbinfer.com/ on how to install" >&2
    exit 1
  fi
  if $OPT_QUIET; then
    ninja debug >/dev/null
  else
    ninja debug
  fi
  ninja -t compdb compile_obj > build/debug-compilation-database.json
  if $OPT_QUIET; then
    infer capture --no-progress-bar --compilation-database build/debug-compilation-database.json
    infer analyze --no-progress-bar
  else
    infer capture --compilation-database build/debug-compilation-database.json
    infer analyze
  fi
elif $OPT_G; then
  if $OPT_QUIET; then
    ninja debug >/dev/null
  else
    ninja debug
  fi
else
  if $OPT_QUIET; then
    ninja release >/dev/null
  else
    ninja release
  fi
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
