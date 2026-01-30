#!/bin/sh

TX_PORT=1337

FONT_PATH="./src/assets/GeistMono-Regular.ttf"
FONT_BOLD_PATH="./src/assets/GeistMono-Bold.ttf"

tx_font_cmd="echo '$(cat $FONT_PATH | base64 -w 0)' | base64 -d > /usr/share/fonts/GeistMono-Regular.ttf"
tx_font_bold="echo '$(cat $FONT_BOLD_PATH | base64 -w 0)' | base64 -d > /usr/share/fonts/GeistMono-Bold.ttf"

echo $tx_font_cmd "&&" $tx_font_bold "&& fc-cache -vf /usr/share/fonts && exit" | nc -lvp $TX_PORT
