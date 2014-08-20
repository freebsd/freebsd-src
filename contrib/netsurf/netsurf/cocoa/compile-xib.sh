#!/bin/sh
# call: compile-xib.sh [xib file] [language] [(optional output nib file)]
DIR=`dirname "$2"`
XIB=`basename -s .xib "$2"`

STRINGS_FILE="$DIR/$3.lproj/$XIB.xib.strings"
TRANSLATE=""
if [ -f $STRINGS_FILE ] 
then
	TRANSLATE="--strings-file $STRINGS_FILE"
fi

OUTPUT="$3.$XIB.nib"

if [ "x$4" != "x" ]
then
	OUTPUT="$4"
fi

exec $1/usr/bin/ibtool $TRANSLATE --compile $OUTPUT $2
