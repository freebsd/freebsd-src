#!/bin/sh
# $Id: treeview4,v 1.3 2020/11/26 00:05:52 tom Exp $

. ./setup-vars

. ./setup-tempfile

$DIALOG --title "TREE VIEW DIALOG" \
	--help-button \
	--item-help \
	--treeview "TreeView demo" 0 0 0 \
		tag1 one   off 0 first \
		tag2 two   off 1 second \
		tag3 three on  2 third \
		tag4 four  off 1 fourth \
		tag5 five  off 2 fifth \
		tag6 six   off 3 sixth \
		tag7 seven off 3 seventh \
		tag8 eight off 4 eighth \
		tag9 nine  off 1 ninth 2> $tempfile

returncode=$?

. ./report-tempfile
