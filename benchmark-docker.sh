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
    run_test "$size"
  done
}

run_test() {
  size=$1
  echo "Testing $size"

  TMPFILE=$(mktemp)
  fallocate "$TMPFILE" -l "$size"
  trap 'rm $TMPFILE' RETURN

  echo "Resetting repo"
  sleep 1
  tmux send-keys -t 2 'nfd'
  sleep 1
  rm -rf /var/lib/ndn/repo/*
  tmux send-keys -t 1 './build/ndn-repo-ng -c ./repo-0.conf'

  sleep 2

  ./build/tools/ndnputfile -D -s 8192 $OPTARGS /example/repo "/example/data/1" "$TMPFILE"
  code=$?

  if [ $code != 0 ]; then
    echo "Put file failed. closing this test"
    tmux send-keys -t 1 ''
    tmux send-keys -t 2 ''
    return 1
  fi

  # Run test round
  for _ in $(seq 1 $REPEAT); do
    restart_nfd_repo
    starttime=$(date +%s.%N)
    ./build/tools/ndngetfile /example/data/1 &>/dev/null
    endtime=$(date +%s.%N)
    elapsed=$(echo "$endtime -$starttime" | bc)
    echo -e "$size\\t$code\\t$elapsed" | tee -a "$LOGFILE"
  done

  tmux send-keys -t 1 ''
  tmux send-keys -t 2 ''
}

restart_nfd_repo() {
  tmux send-keys -t 1 ''
  tmux send-keys -t 2 ''
  sleep 1
  tmux send-keys -t 2 'nfd'
  sleep 1
  tmux send-keys -t 1 './build/ndn-repo-ng -c ./repo-0.conf >/dev/null'
  sleep 1
}

prepare
run_all
