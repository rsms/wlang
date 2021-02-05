#!/bin/bash -e
cd "$(dirname "$0")"

BUILDDEPS=$PWD/builddeps

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
  -h|-help|--help)        OPT_HELP=true; shift ;;
  -config|--config)       OPT_CONFIG=true; shift ;;
  -clean|--clean)         OPT_CLEAN=true; shift ;;
  -a|-analyze|--analyze)  OPT_ANALYZE=true; shift ;;
  -t|-test|--test)        OPT_TEST=true; shift ;;
  -q|-quiet|--quiet)      OPT_QUIET=true; shift ;;
  -g)                     OPT_G=true; shift ;;
  *)
    echo "$0: Unknown option $1" >&2
    USAGE_EXIT_CODE=1
    OPT_HELP=true; shift ;;
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

# _checksum [<file>]
# Prints the sha1 sum of file's content or stdin if <file> is not given
_checksum() {
  sha1sum "$@" | cut -f 1 -d ' '
}

_ninja() {
  if $OPT_QUIET; then
    ninja "$@" >/dev/null
  else
    ninja "$@"
  fi
}

# _find_llvm attempts to find the path to llvm binaries, first considering PATH.
_find_llvm() {
  LLVM_PATH=$(command -v llvm-cov)
  if [[ "$LLVM_PATH" != "" ]]; then
    echo $(dirname "$LLVM_PATH")
    return 0
  fi
  test_paths=()
  if (command -v clang >/dev/null); then
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

DOWNLOAD="$BUILDDEPS"/download

# _fetch url sha1sum [outfile]
# Download outfile from url. If outfile is not given DOWNLOAD/(basename url) is used.
# If outfile exists then only download if the checksum does not match.
_fetch() {
  local url="$1" ; shift
  local checksum="$1" ; shift
  local outfile="$2"
  [ -n "$outfile" ] || outfile="$DOWNLOAD/$(basename "$url")"
  while [ ! -e "$outfile" ] || [ "$(_checksum "$outfile")" != "$checksum" ]; do
    if [ -n "$did_download" ]; then
      echo "Checksum for $outfile failed" >&2
      echo "  Actual:   $(_checksum "$outfile")" >&2
      echo "  Expected: $checksum" >&2
      return 1
    fi
    rm -rf "$outfile"
    echo "fetch $url"
    mkdir -p "$(dirname "$outfile")"
    curl -L --progress-bar -o "$outfile" "$url"
    did_download=y
  done
}

_install_infer() {
  local VERSION=1.0.0
  rm -rf "$BUILDDEPS"/infer "$BUILDDEPS"/.infer
  mkdir -p "$BUILDDEPS"
  echo "installing infer"
  local filename
  case $(uname -s)-$(uname -m) in
    Darwin-x86_64) filename="infer-osx-v$VERSION.tar.xz" ;;
    Linux-x86_64)  filename="infer-linux64-v$VERSION.tar.xz" ;;
    *) echo "unsupported platform and/or arch: $(uname -s)-$(uname -m)" >&2; return 1 ;;
  esac
  _fetch "https://github.com/facebook/infer/releases/download/v$VERSION/$filename" \
         a3ac72241bb9c53e78152ceb7681a08fd5d8298c
  echo "extracting $filename"
  XZ_OPT='-T0' tar -C "$BUILDDEPS" -xJf "$DOWNLOAD/$filename"
  mv "$BUILDDEPS"/infer* "$BUILDDEPS"/infer
}

_clean() {
  rm -rf build
}

_config() {
  if $OPT_CONFIG || [ config.sh -nt build.ninja ] || [ build.in.ninja -nt build.ninja ]; then
    ./config.sh
  fi
}

_gen_debug_compdb() {
  ninja -t compdb compile_obj > build/compilation-database.json
  python3 misc/filter-compdb.py build/compilation-database.json build/obj/dev/ > \
    build/debug-compilation-database.json
}

_analyze() {
  local infer="$BUILDDEPS"/infer/bin/infer
  [ -x "$infer" ] || _install_infer
  _ninja debug
  _gen_debug_compdb
  rm -rf build/infer  # clashes with dev.sh -a
  local infer_args=()
  $OPT_QUIET && infer_args+=( --no-progress-bar )
  "$infer" capture "${infer_args[@]}" --compilation-database build/debug-compilation-database.json
  $OPT_QUIET || echo "analyzing"
  "$infer" analyze "${infer_args[@]}"
}

_test() {
  # https://clang.llvm.org/docs/SourceBasedCodeCoverage.html
  local LLVM_PATH=$(_find_llvm)
  echo "PATH $PATH"
  _ninja test  # build test product

  # run built-in unit tests
  LLVM_PROFILE_FILE=build/unit-tests.profraw \
  W_TEST_MODE=exclusive \
    ./build/wp.test
  # LLVM_PROFILE_FILE=build/ast1.profraw build/wp.test example/factorial.w >/dev/null

  # index coverage data
  "$LLVM_PATH/llvm-profdata" merge -sparse -o build/test.profdata build/*.profraw
  rm build/*.profraw

  # print report
  local cov_shared_args=( \
    -ignore-filename-regex='dlmalloc\.[hc]' \
    -ignore-filename-regex='sds\.[hc]' \
    -ignore-filename-regex='.+_test\.[hc]' \
    -ignore-filename-regex='test\.[hc]' \
    -instr-profile=build/test.profdata \
    build/wp.test \
  )

  # print report summary on stdout
  "$LLVM_PATH/llvm-cov" report "${cov_shared_args[@]}"

  # generate full report as HTML at ./build/cov/
  rm -rf build/cov
  mkdir -p build/cov
  "$LLVM_PATH/llvm-cov" show \
    "${cov_shared_args[@]}" \
    -show-line-counts-or-regions \
    -format=html \
    -tab-size=4 \
    -output-dir=build/cov

  echo "Report generated at build/cov/index.html"
  if [[ "$(lsof +c 0 -sTCP:LISTEN -iTCP:8186 | tail -1)" == "" ]]; then
    echo "To view in a live-reloading web server, run this in a separate terminal."
    echo "The report will update live as this script is re-run."
    echo "  serve-http -p 8186 '$PWD/build/cov'"
  fi
}

$OPT_CLEAN && _clean
_config
if $OPT_ANALYZE; then
  _analyze
elif $OPT_TEST; then
  _test
elif $OPT_G; then
  _ninja debug
else
  _ninja release
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
