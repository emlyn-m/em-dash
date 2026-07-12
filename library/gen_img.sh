#!/bin/sh

set -eu

HELP_MSG="\x1b[38;5;139m\x1b[1mHELP:\x1b[0m: $0 [ image_filepath ] [ icon_name ]"

if [ $# -ne 2 ]; then
	echo -e $HELP_MSG
	exit 1
fi

img_path=$1
header_path=$2
icon_name=$(basename $header_path)
icon_name=${icon_name%.*}
echo -e "\x1b[38;5;139m\x1b[1mINFO: \x1b[0m Converting <$img_path> into <$icon_name> (<$header_path>)"

gdk-pixbuf-csource --raw --name=$icon_name $img_path > $header_path.tmp
echo "#include <glib.h>" > $header_path
cat $header_path.tmp >> $header_path
rm $header_path.tmp
