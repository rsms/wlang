#!/bin/bash -e
cd "$(dirname "$0")"

BUILDDEPS=$PWD/builddeps

OPT_HELP=false
OPT_TIME=false
OPT_LLDB=false
OPT_ANALYZE=false
FWD_ARGS=()

while [[ $# -gt 0 ]]; do
  case "$1" in
  -h|-help|--help)       OPT_HELP=true; shift ;;
  -time|--time)          OPT_TIME=true; shift ;;
  -lldb|--lldb)          OPT_LLDB=true; shift ;;
  -a|-analyze|--analyze) OPT_ANALYZE=true; shift ;;
  --)                    break; shift;;
  -*)                    echo "$0: Unknown option $1" >&2; exit 1 ;;
  *)                     FWD_ARGS+=( "$1" ); shift ;;
  esac
done
if $OPT_HELP; then
  echo "Build and run during development with automatic reloading on source changes."
  echo "usage: $0 [options] [--] [<runarg> ...]"
  echo "options:"
  echo "  -h, -help   Show help"
  echo "  -time       Run release-build program with performance timer."
  echo "              Redirects stdio to /dev/null."
  echo "  -lldb       Run debug build in lldb."
  echo "  -analyze    Run './build.sh -analyze', watching source files."
  echo "<runarg>s are passed to the program"
  exit 1
fi

FWD_ARGS+=( "$@" )
if [ ${#FWD_ARGS[@]} -eq 0 ]; then
  FWD_ARGS+=( example/factorial.w )
fi

if ! (which fswatch >/dev/null); then
  echo "Missing fswatch. See http://emcrisostomo.github.io/fswatch/" >&2
  exit 1
fi


pidfile=build/dev-pidfile
analyze_pidfile=build/dev_analyze-pidfile

if $OPT_ANALYZE; then
  if [ -f "$analyze_pidfile" ]; then
    kill "$(cat "$analyze_pidfile")" 2>/dev/null
    rm -f "$analyze_pidfile"
  fi
else
  if [ -f "$pidfile" ]; then
    kill "$(cat "$pidfile")" 2>/dev/null
    rm -f "$pidfile"
  fi
fi


function dev_run_exec {
  set +e
  ASAN_OPTIONS=detect_stack_use_after_return=1 W_TEST_MODE=on ./build/wp.g "$@"
  DEV_RUN_STATUS=$?
  set -e
  # if [ $DEV_RUN_STATUS -ne 0 ] && [ $DEV_RUN_STATUS -ne 1 ]; then
  #   echo "—————————————————————————————————————————————————————————————————————"
  #   echo "CRASH ($DEV_RUN_STATUS) -- starting debugger"
  #   echo lldb -bo r ./build/wp.g "$@"
  #   ASAN_OPTIONS=detect_stack_use_after_return=1 \
  #     lldb -bo r ./build/wp.g "$@"
  # fi
}

function dev_run {
  set +e
  pid=
  if $OPT_TIME; then
    time ./build/wp "$@" >/dev/null 2>&1 &
    pid=$!
  else
    if $OPT_LLDB; then
      echo lldb -bo r ./build/wp.g "$@"
      ASAN_OPTIONS=detect_stack_use_after_return=1 \
      W_TEST_MODE=on \
        lldb -bo r ./build/wp.g "$@"
    else
      dev_run_exec "$@" &
      pid=$!
    fi
  fi
  echo $pid > "$pidfile"
  wait $pid
  rm -f "$pidfile"
  set -e
}

function dev_start {
  if $OPT_TIME; then
    ./build.sh && dev_run "$@"
  else
    ./build.sh -g && dev_run "$@"
  fi
}

function dev_stop {
  set +e
  if [ -f "$pidfile" ]; then
    pid=$(cat "$pidfile")
    kill "$pid" 2>/dev/null
    wait "$pid"
    rm -f "$pidfile"
  fi
  set -e
}

function dev_analyze_stop {
  set +e
  if [ -f "$analyze_pidfile" ]; then
    pid=$(cat "$analyze_pidfile")
    kill "$pid" 2>/dev/null
    wait "$pid"
    rm -f "$analyze_pidfile"
  fi
  set -e
}

function dev_analyze_fswatch {
  set +e
  while true; do  # outer loop since sometimes fswatch just dies
    fswatch \
      -0 \
      --extended \
      --exclude='.*' --include='\.(c|h)$' \
      --recursive \
      src \
    | while read -d "" filename; do
      # echo "file changed: ${filename}"
      echo $filename >> build/analyze_changed_files.txt
    done
  done
  set -e
}

function dev_analyze_start {
  dev_analyze_fswatch &
  ANALYZE_PID=$!
  mkdir -p build
  echo $ANALYZE_PID > "$analyze_pidfile"
  # wait $ANALYZE_PID  # wait for analyze to finish
  # rm -f "$analyze_pidfile"
}


function dev_cleanup {
  if $OPT_ANALYZE; then
    dev_analyze_stop
  else
    dev_stop
  fi
  echo ""
}
trap dev_cleanup EXIT
trap exit SIGINT  # make sure we can ctrl-c in the while loop


_gen_debug_compdb() {
  ninja -t compdb compile_obj > build/compilation-database.json
  python3 misc/filter-compdb.py build/compilation-database.json build/obj/dev/ > \
    build/debug-compilation-database.json
}

if $OPT_ANALYZE; then
  ninja debug
  echo "Running analyzer, starting with an analysis of all uncommitted files."
  echo "Run 'infer explore' in a separate shell to explore issues."

  infer="$BUILDDEPS"/infer/bin/infer

  if [ ! -x "$infer" ]; then
    echo "Infer not installed. Run ./build.sh -a" >&2
    exit 1
  fi

  rm -f build/analyze_changed_files.txt
  echo -n "" > build/analyze_changed_files.txt
  rm -rf build/infer # clashes with build.sh -a

  dev_analyze_start
  _gen_debug_compdb
  git status --porcelain | sed 's/^...//' > build/git-dirty-files.txt
  "$infer" capture --no-progress-bar --compilation-database build/debug-compilation-database.json
  echo "Initial analysis of \"git dirty\" sources..."
  "$infer" analyze --changed-files-index build/git-dirty-files.txt
  echo "Analyzer watching for file changes..."

  set +e

  while true; do
    while true; do
      # echo "checking for file changes"
      rm -f build/analyze_changed_files2.txt
      if mv build/analyze_changed_files.txt build/analyze_changed_files2.txt 2>/dev/null; then
        if [ -s build/analyze_changed_files2.txt ]; then
          # there are changes
          break
        fi
      fi
      # blinking dot to keep ssh and iterm responsive
      echo -n -e ".\x1b[1D" # print "." and move cursor left by 1
      sleep 0.5
      echo -n -e "\x1b[0K" # clear from cursor until end of line
      sleep 0.5
    done
    ninja debug >/dev/null && \
    _gen_debug_compdb
    git status --porcelain | sed 's/^...//' > build/git-dirty-files.txt
    "$infer" capture --changed-files-index build/analyze_changed_files2.txt \
                     --no-progress-bar \
                     --compilation-database build/debug-compilation-database.json && \
    "$infer" analyze --progress-bar-style plain \
                     --changed-files-index build/analyze_changed_files2.txt
  done
  set -e
else
  IS_FIRST_RUN=true
  while true; do
    echo -e "\x1bc"  # clear screen ("scroll to top" style)
    if $IS_FIRST_RUN; then
      IS_FIRST_RUN=false
      echo "Tip: Run './dev.sh -analyze' in a second shell for incremental code analysis."
    fi
    dev_start "${FWD_ARGS[@]}"
    if $OPT_LLDB; then
      echo "Watching files for changes..."
    fi
    fswatch \
      --one-event \
      --latency=0.2 \
      --extended \
      --exclude='.*' --include='\.(c|h|w|sh|lisp)$' \
      --recursive \
      src example config.sh
    echo "———————————————————— restarting ————————————————————"
    dev_stop
  done
fi
