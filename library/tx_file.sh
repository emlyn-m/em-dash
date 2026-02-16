# Serverside

FPATH_LOCAL="/home/emlyn/pets/kindle/build_kindlepw2/dash"
FPATH_REMOTE="/dash_test"
FPATH_REMOTE_BOOKS="/mnt/us/documents"
BOOK_NAME="dash"

TX_PORT=1338
STDOUT_IP=10.21.83.11
STDOUT_PORT=1339

book_content="\"#!/bin/sh\n# Name: ; run $BOOK_NAME\n# Author: winter\n\n rm /tmp/f; mkfifo /tmp/f; source /.env; source /.env.config; cat /tmp/f | $FPATH_REMOTE 2>&1 | nc -w 15 $STDOUT_IP $STDOUT_PORT > /tmp/f \""
echo -e "\x1b[38;5;139m\x1b[1mINFO:\x1b[0m ready to transmit on port $TX_PORT"
echo "mount / -o remount,rw && rm -f $FPATH_REMOTE && rm -f $FPATH_REMOTE_BOOKS/$BOOK_NAME.sh && echo '$(cat $FPATH_LOCAL | base64 -w 0)' | base64 -d > $FPATH_REMOTE && chmod +x $FPATH_REMOTE && echo -e $book_content > $FPATH_REMOTE_BOOKS/$BOOK_NAME.sh && exit" | nc -lvp $TX_PORT 2>/dev/null

echo -e "\n\x1b[38;5;139m\x1b[1mINFO:\x1b[0m listening for stdout/stderr on port $STDOUT_PORT"
nc -lvp $STDOUT_PORT 2>/dev/null
