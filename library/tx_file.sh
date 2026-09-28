# Serverside

FPATH_LOCAL="./build_kindlepw2/dash"
FPATH_REMOTE="/dash_test"
FPATH_REMOTE_BOOKS="/mnt/us/documents"
BOOK_NAME="dash"

TX_PORT=1337
STDOUT_IP=$(hostname -i | awk '{ print $1 }')
STDOUT_PORT=1339

# 1&>2 needed - text will corrupt display otherwise
book_content="\"#!/bin/sh\n# Name: ; run $BOOK_NAME\n# Author: winter\n\n mount -o rw,remount / && $FPATH_REMOTE 1&>2 \""
echo -e "\x1b[38;5;139m\x1b[1mINFO:\x1b[0m ready to transmit on port $TX_PORT"
echo "mount / -o remount,rw && rm -f $FPATH_REMOTE && rm -f $FPATH_REMOTE_BOOKS/$BOOK_NAME.sh && echo '$(cat $FPATH_LOCAL | base64 -w 0)' | base64 -d > $FPATH_REMOTE && chmod +x $FPATH_REMOTE && echo -e $book_content > $FPATH_REMOTE_BOOKS/$BOOK_NAME.sh && exit" | nc -lvp $TX_PORT 2>/dev/null

echo -e "\n\x1b[38;5;139m\x1b[1mINFO:\x1b[0m complete <3 enjoy!"
