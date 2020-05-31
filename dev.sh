#!/bin/bash -e
cd "$(dirname "$0")"

OPT_HELP=false
OPT_TIME=false

while [[ $# -gt 0 ]]; do
  case "$1" in
  -h|-help|--help)
    OPT_HELP=true
    shift
    ;;
  --)
    shift
    break
    ;;
  -time|--time)
    OPT_TIME=true
    shift
    ;;
  -*)
    echo "$0: Unknown option $1" >&2
    OPT_HELP=true
    shift
    ;;
  *)
    break
    ;;
  esac
done
if $OPT_HELP; then
  echo "Build and run during development with automatic reloading on source changes."
  echo "usage: $0 [options] [--] [<runarg> ...]"
  echo "options:"
  echo "  -h, -help   Show help"
  echo "  -time       Run release-build program with performance timer."
  echo "              Redirects stdio to /dev/null."
  echo "<runarg>s are passed to the program"
  exit 1
fi


if ! (which fswatch >/dev/null); then
  echo "Missing fswatch. See http://emcrisostomo.github.io/fswatch/" >&2
  exit 1
fi


pidfile=build/dev-pidfile
if [ -f "$pidfile" ]; then
  kill "$(cat "$pidfile")" 2>/dev/null
  rm "$pidfile"
fi


function dev_run_exec {
  set +e
  ASAN_OPTIONS=detect_stack_use_after_return=1 ./build/wp.g "$@"
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
    dev_run_exec "$@" &
    # dev_run_exec "$@"
    pid=$!
  fi
  echo $pid > "$pidfile"
  wait $pid
  rm -f "$pidfile"
  set -e
}


function dev_start {
  if $OPT_TIME; then
    if ./build.sh; then
      dev_run "$@"
    fi
  else
    if ./build.sh -g; then
      dev_run "$@"
    fi
  fi
}


function dev_stop {
  if [ -f "$pidfile" ]; then
    set +e
    pid=$(cat "$pidfile")
    kill "$pid" 2>/dev/null
    wait "$pid"
    set -e
    rm -f "$pidfile"
  fi
}


function dev_cleanup {
  dev_stop
}
trap dev_cleanup EXIT
trap exit SIGINT  # make sure we can ctrl-c in the while loop


while true; do
  echo -e "\x1bc"  # clear screen ("scroll to top" style)
  dev_start "$@"
  fswatch -1 -l 0.2 -E --exclude='.*' --include='\.(c|h|w|sh)$' src example config.sh
  echo "———————————————————— restarting ————————————————————"
  dev_stop
done
