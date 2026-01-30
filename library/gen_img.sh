#!/usr/bin bash

set -eu

HELP_MSG="usage: $0 [ image_filepath ] [ icon_name ]"

if [ $# -ne 2 ]; then
	echo $HELP_MSG
	exit 1
fi

img_path=$1
header_path=$2
icon_name=$(basename $header_path)
icon_name=${icon_name%.*}
echo "Converting <$img_path> into <$icon_name> (<$header_path>)"

gdk-pixbuf-csource --raw --name=$icon_name $img_path > $header_path.tmp
echo "#include <glib.h>" > $header_path
cat $header_path.tmp >> $header_path
rm $header_path.tmp
