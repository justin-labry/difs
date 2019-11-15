#!/bin/bash

set -eo pipefail

print_msg() {
    msg=$1
    tmux select-pane -t 0
    tmux resize-pane -Z
    tmux send-keys "clear; echo -e '${msg}' | tbraille -f Arial -s 20"
    sleep 5
    tmux resize-pane -Z
    tmux send-keys "clear"
}

setup() {
    sleep 1

    # Make 4*2 screen vertical order
    tmux split-pane -h
    tmux split-pane -t 0 -h
    tmux split-pane -t 2 -h
    tmux split-pane -t 0 -v
    tmux split-pane -t 2 -v
    tmux split-pane -t 4 -v
    tmux split-pane -t 6 -v
    sleep 1

    # p0 main console

    # p1 nfd
    tmux send-keys -t 1 'nfd'
    sleep 5

    # p2 repo-0
    tmux send-keys -t 2 './build/ndn-repo-ng -c ./repo-0.conf'
    tmux send-keys -t 3 'watch tree -FL 3 /var/lib/ndn/repo/0'
    sleep 3

    # p4 repo-1
    tmux send-keys -t 4 './build/ndn-repo-ng -c ./repo-1.conf'
    tmux send-keys -t 5 'watch tree -FL 3 /var/lib/ndn/repo/1'
    sleep 3

    # w4 repo-2
    tmux send-keys -t 6 './build/ndn-repo-ng -c ./repo-2.conf'
    tmux send-keys -t 7 'watch tree -FL 3 /var/lib/ndn/repo/2'
    sleep 3


    # Scene 1. Put file, data - 0, manifest - 1
    print_msg "Scenario 1\nPUT"
    sleep 3
    tmux send-keys -t 0 'build/tools/ndnputfile -v /example/repo/ /example/data/1 ./COPYING.md'
    sleep 10

    # Scene 2. print manifest
    print_msg "Scenario 2\nMANIFEST"
    sleep 3
    tmux send-keys -t 0 'jq . /var/lib/ndn/repo/1/manifest/4d8dd1244f189e55795e7beeda192c1451c19ff9'
    sleep 10

    # Scene 3. ndngetfile
    tmux send-keys -t 2 ''
    tmux send-keys -t 4 ''
    print_msg "Scenario 3\nGET"
    sleep 3
    tmux send-keys -t 0 'build/tools/ndngetfile -v /example/data/1'
    sleep 10

    tmux kill-session
    killall nfd
}

rm -rf /var/lib/ndn/repo/*

tmux new -ds difs
setup &
pid=$!
trap 'kill $pid; killall nfd' EXIT
tmux attach -t difs
