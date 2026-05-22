#!/bin/sh

set -e

gen_tx_cmd() {
    if [ ! -f $1 ]; then
        echo -e "\x1b[38;5;139m\x1b[1mERR:\x1b[0m file \"$1\" does not exist" >&2
        exit 1
    fi
    echo "echo '$(cat $1 | base64 -w 0)' | base64 -d > $2/$(basename $1) && "
}

HELP_MSG="\x1b[38;5;139m\x1b[1mHELP:\x1b[0m tx_assets.sh [data files] --docs [document files] --fonts [font files]"
REMOTE_DATA_PATH="/"
REMOTE_DOC_PATH="/mnt/us/documents"
REMOTE_FONT_PATH="/usr/share/fonts"
TX_PORT=1337

if [ "$#" -eq 0 ]; then
    echo -e $HELP_MSG
    exit 1
fi

full_tx_cmd="mount -o rw,remount / &&"
active_mode_path=$REMOTE_DATA_PATH
active_mode_name=""
while [[ "$#" -gt 0 ]]; do
    case "$1" in
        --fonts)
            if [ -n "$2" ]; then
                active_mode_name=" font"
                active_mode_path=$REMOTE_FONT_PATH
            else
                echo -e "\x1b[38;5;139m\x1b[1mERR:\x1b[0m --fonts without any font files" >&2
                exit 1
            fi
            ;;
        --docs)
            if [ -n "$2" ]; then
                active_mode_name=" document"
                active_mode_path=$REMOTE_DOC_PATH
            else
                echo -e "\x1b[38;5;139m\x1b[1mERR:\x1b[0m --docs without any doc files" >&2
            fi
            ;;
        *)
            echo -e "\x1b[38;5;139m\x1b[1mINFO:\x1b[0m loading$active_mode_name $1 with name \"$(basename $1)\""
            full_tx_cmd="$full_tx_cmd$(gen_tx_cmd $1 $active_mode_path)"
            ;;
    esac
    shift # shift to the next argument or value

done

if [ -z "$full_tx_cmd" ]; then
    echo -e "\x1b[38;5;139m\x1b[1mERR:\x1b[0m no files specified"
    exit 1
fi

full_tx_cmd="${full_tx_cmd}fc-cache -vf $REMOTE_FONT_PATH && exit"
echo -e "\x1b[38;5;139m\x1b[1mDBG: \x1b[0m using \"$full_tx_cmd\""
echo -e "\x1b[38;5;139m\x1b[1mINFO:\x1b[0m ready transmit on port $TX_PORT"
echo $full_tx_cmd | nc -lvp $TX_PORT 2>/dev/null
