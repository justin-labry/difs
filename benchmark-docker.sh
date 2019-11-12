#!/bin/bash

LOGFILE="$1"; shift
OPTARGS=$*

SIZES=(200000000 700000000 1000000000)

REPEAT=10


closing() {
  tmux send-keys -t 2 'nfd-stop'
  sleep 0.5
  tmux kill-pane -t 2
  tmux kill-pane -t 1
}

prepare() {
  tmux split-pane
  tmux split-pane -h

  mkdir -p /var/lib/ndn/repo
  tmux select-pane -t 0

  trap closing EXIT
}

run_all() {
  for size in "${SIZES[@]}"; do
    for _ in $(seq $REPEAT); do
      run_test "$size"
    done
  done
}

run_test() {
  TMPFILE=$(mktemp)
  echo "Using $TMPFILE"
  trap 'rm -f $TMPFILE' RETURN

  size=$1
  echo "Testing $size"
  fallocate "$TMPFILE" -l "$size"

  echo "Resettings repo"
  sleep 1
  tmux send-keys -t 2 'nfd'
  sleep 1
  rm -rf /var/lib/ndn/repo/*
  tmux send-keys -t 1 './build/ndn-repo-ng -c ./repo-0.conf'

  sleep 2

  starttime=$(date +%s.%N)
  ./build/tools/ndnputfile -D -s 8192 $OPTARGS /example/repo "/example/data/1" "$TMPFILE"
  code=$?
  endtime=$(date +%s.%N)

  tmux send-keys -t 1 ''
  tmux send-keys -t 2 ''

  # elapsed=$(( endtime - starttime ))
  elapsed=$(echo "$endtime -$starttime" | bc)
  echo -e "$size\\t$code\\t$elapsed" | tee -a "$LOGFILE"
}

prepare
run_all
