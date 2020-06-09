#!/bin/bash -e
cd "$(dirname "$0")"

OPT_HELP=false
OPT_CONFIG=false
OPT_G=false
OPT_CLEAN=false
OPT_ANALYZE=false
OPT_QUIET=false
OPT_TEST=false
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
  -t|-test|--test)
    OPT_TEST=true
    shift
    ;;
  -q|-quiet|--quiet)
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
  echo "  -t, -test     Run tests, including code coverage analysis."
  echo "  -q, -quiet    Only print errors."
  exit $USAGE_EXIT_CODE
fi

function fn_ninja {
  if $OPT_QUIET; then
    ninja "$@" >/dev/null
  else
    ninja "$@"
  fi
}

# fn_find_llvm attempts to find the path to llvm binaries, first considering PATH.
function fn_find_llvm {
  LLVM_PATH=$(which llvm-cov)
  if [[ "$LLVM_PATH" != "" ]]; then
    echo $(dirname "$LLVM_PATH")
    return 0
  fi
  test_paths=()
  if (which clang >/dev/null); then
    CLANG_PATH=$(clang -print-search-dirs | grep programs: | awk '{print $2}' | sed 's/^=//')
    if [[ "$CLANG_PATH" != "" ]]; then
      test_paths+=( "$CLANG_PATH" )
    fi
  fi
  if [[ "$OSTYPE" == "darwin"* ]]; then
    test_paths+=( /Library/Developer/CommandLineTools/usr/bin )
  # elif [[ "$OSTYPE" == "linux"* ]]; then
  #   echo "linux"
  # elif [[ "$OSTYPE" == "cygwin" ]] || \
  #      [[ "$OSTYPE" == "msys" ]] || \
  #      [[ "$OSTYPE" == "win32" ]] || \
  #      [[ "$OSTYPE" == "win64" ]]
  # then
  #   echo "win"
  fi
  for path in ${test_paths[@]}; do
    if [[ -f "$path/llvm-cov" ]]; then
      echo "$path"
      return 0
    fi
  done
  echo "llvm not found in PATH. Also searched:" >&2
  for path in ${test_paths[@]}; do
    echo "  $path" >&2
  done
  return 1
}

if $OPT_CLEAN; then
  rm -rf build
fi

if $OPT_CONFIG || [ config.sh -nt build.ninja ] || [ build.in.ninja -nt build.ninja ]; then
  ./config.sh
fi

if $OPT_ANALYZE; then
  if ! (which infer >/dev/null); then
    echo "'infer' not found in PATH. See https://fbinfer.com/ on how to install" >&2
    exit 1
  fi
  fn_ninja debug
  fn_ninja -t compdb compile_obj > build/debug-compilation-database.json
  if $OPT_QUIET; then
    infer capture --no-progress-bar --compilation-database build/debug-compilation-database.json
    infer analyze --no-progress-bar
  else
    infer capture --compilation-database build/debug-compilation-database.json
    infer analyze
  fi
elif $OPT_TEST; then
  # https://clang.llvm.org/docs/SourceBasedCodeCoverage.html
  LLVM_PATH=$(fn_find_llvm)
  echo "PATH $PATH"
  fn_ninja test  # build test product

  # run built-in unit tests
  LLVM_PROFILE_FILE=build/unit-tests.profraw W_TEST_MODE=exclusive build/wp.test
  LLVM_PROFILE_FILE=build/ast1.profraw build/wp.test example/factorial.w >/dev/null

  # index coverage data
  "$LLVM_PATH/llvm-profdata" merge -sparse -o build/test.profdata build/*.profraw
  rm build/*.profraw

  # generate & print report
  "$LLVM_PATH/llvm-cov" report build/wp.test -instr-profile=build/test.profdata
  # llvm-cov show build/wp.test -instr-profile=build/test.profdata  # extremely verbose

  mkdir -p build/cov
  "$LLVM_PATH/llvm-cov" show build/wp.test \
    -instr-profile=build/test.profdata \
    -ignore-filename-regex='dlmalloc.*' \
    -format=html \
    -tab-size=4 \
    -output-dir=build/cov
  echo "Report generated at build/cov/index.html"
  if [[ "$(lsof +c 0 -sTCP:LISTEN -iTCP:8186 | tail -1)" == "" ]]; then
    echo "To view in a live-reloading web server, run this in a separate terminal."
    echo "The report will update live as this script is re-run."
    echo "  serve-http -p 8186 '$PWD/build/cov'"
  fi



elif $OPT_G; then
  fn_ninja debug
else
  fn_ninja release
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
