#!/bin/bash

LOGFILE="$1"

START=10  # 1kiB
END=30  # 1GiB

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
  tmux send-keys -t 1 'sudo echo 1'
  tmux send-keys -t 2 'sudo echo 1'

  trap closing EXIT
}

run_all() {
  sudo echo 1
  echo -n "Prepare and press enter here"
  read

  for i in $(seq $START $END); do
    for _ in $(seq 1 $REPEAT); do
      run_test "$i"
    done
  done
}

run_test() {
  TMPFILE=$(mktemp)
  echo "Using $TMPFILE"
  trap 'rm -f $TMPFILE' RETURN

  level=$1
  size=$(( 1 << level ))
  echo "Testing $size"
  fallocate "$TMPFILE" -l "$size"

  echo "Resettings repo"
  sleep 1
  tmux send-keys -t 2 'nfd-start'
  sleep 1
  sudo rm -rf /var/lib/ndn/repo/*
  tmux send-keys -t 1 'sudo ./build/ndn-repo-ng -c ./repo-0.conf'

  sleep 2

  starttime=$(date +%s.%N)
  ./build/tools/ndnputfile /example/repo/0 "/example/data/1" "$TMPFILE"
  code=$?
  endtime=$(date +%s.%N)

  tmux send-keys -t 1 ''
  tmux send-keys -t 2 'nfd-stop'

  # elapsed=$(( endtime - starttime ))
  elapsed=$(echo "$endtime -$starttime" | bc)
  echo -e "$size\\t$code\\t$elapsed" | tee -a "$LOGFILE"
}

prepare
run_all
