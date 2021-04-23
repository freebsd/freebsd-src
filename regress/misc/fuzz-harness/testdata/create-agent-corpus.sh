#!/bin/sh

# Exercise ssh-agent to generate fuzzing corpus

# XXX assumes agent hacked up with sk-dummy.o and ssh-sk.o linked directly
#     and dumping of e->request for each message.

set -xe
SSH_AUTH_SOCK=$PWD/sock
rm -f agent-[0-9]* $SSH_AUTH_SOCK
export SSH_AUTH_SOCK
../../../../ssh-agent -D -a $SSH_AUTH_SOCK &
sleep 1
AGENT_PID=$!
trap "kill $AGENT_PID" EXIT

PRIV="id_dsa id_ecdsa id_ecdsa_sk id_ed25519 id_ed25519_sk id_rsa"

# add keys
ssh-add $PRIV

# sign
ssh-add -T *.pub

# list
ssh-add -l

# remove individually
ssh-add -d $PRIV

# re-add with constraints
ssh-add -c -t 3h $PRIV

# delete all
ssh-add -D

# attempt to add a PKCS#11 token
ssh-add -s /fake || :

# attempt to delete PKCS#11
ssh-add -e /fake || :

ssh-add -L

