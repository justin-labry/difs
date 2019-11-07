#!/bin/bash

LOGFILE="$1"; shift
OPTARGS=$*

START=10  # 1kiB
END=28  # 256MiB

REPEAT=5


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
  for i in $(seq $START $END); do
    run_test "$i"
  done
}

run_test() {
  level=$1
  size=$(( 1 << level ))
  echo "Testing $size"

  TMPFILE=$(mktemp)
  fallocate "$TMPFILE" -l "$size"

  echo "Resetting repo"
  sleep 1
  tmux send-keys -t 2 'nfd'
  sleep 1
  rm -rf /var/lib/ndn/repo/*
  tmux send-keys -t 1 './build/ndn-repo-ng -c ./repo-0.conf'

  sleep 2

  ./build/tools/ndnputfile $OPTARGS /example/repo "/example/data/1" "$TMPFILE"
  code=$?

  if [ $code != 0 ]; then
    echo "Put file failed. closing this test"
    return 1
  fi

  # Run test round
  for _ in $(seq 1 $REPEAT); do
    restart_nfd_repo
    starttime=$(date +%s.%N)
    ./build/tools/ndngetfile /example/data/1
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
  tmux send-keys -t 1 './build/ndn-repo-ng -c ./repo-0.conf'
  sleep 1
}

prepare
run_all
