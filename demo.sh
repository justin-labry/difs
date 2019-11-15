#!/bin/bash

set -eo pipefail

setup() {
    sleep 1

    tmux split-pane -h
    tmux split-pane -v
    tmux split-pane -t 0 -v
    tmux split-pane -t 1 -v
    tmux split-pane -t 4 -v

    # Pane 4 - watch tree - 0
    tmux send-keys -t 4 'watch tree -FL 3 /var/lib/ndn/repo/0'

    # Pane 5 - watch tree - 1
    tmux send-keys -t 5 'watch tree -FL 3 /var/lib/ndn/repo/1'

    # Pane 3 - nfd
    tmux send-keys -t 3 'nfd'
    sleep 1

    # Pane 1 - repo-ng-0
    tmux send-keys -t 1 './build/ndn-repo-ng -c ./repo-0.conf'
    sleep 4

    # Pane 2 - repo-ng-0
    tmux send-keys -t 2 './build/ndn-repo-ng -c ./repo-1.conf'
    sleep 2

    # Pane 0 - run ndnputfile
    tmux select-pane -t 0


    tmux send-keys -t 0 'echo "Hello, World!" > tmp.txt'
    sleep 2
    tmux send-keys -t 0 'cat tmp.txt'

    sleep 3
    tmux send-keys -t 0 'build/tools/ndnputfile -v /example/repo/ /example/data/1 tmp.txt'
    sleep 5
    tmux send-keys -t 0 'build/tools/ndnputfile -v /example/repo/ /example/data/3 tmp.txt'
    sleep 5
    tmux send-keys -t 1 ''
    sleep 2
    tmux send-keys -t 0 'build/tools/ndnputfile -v /example/repo/ /example/data/2 tmp.txt'
    sleep 5

    tmux send-keys -t 0 'clear'
    sleep 1
    tmux send-keys -t 0 'jq . /var/lib/ndn/repo/0/manifest/4d8dd1244f189e55795e7beeda192c1451c19ff9'
    sleep 5

    tmux send-keys -t 0 'clear'
    sleep 1
    tmux send-keys -t 0 'build/tools/ndngetfile -v /example/data/1'
    sleep 5

    tmux kill-window
    killall nfd
}

rm -rf /var/lib/ndn/repo/*

tmux new -ds difs
setup &
tmux attach -t difs
