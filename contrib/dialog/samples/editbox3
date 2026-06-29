#!/bin/sh
# $Id: editbox3,v 1.8 2020/11/26 00:03:58 tom Exp $
# example with extra- and help-buttons

. ./setup-vars

. ./setup-edit

cat << EOF > $input
EOF

$DIALOG --title "EDIT BOX" \
	--extra-button \
	--help-button \
	--fixed-font "$@" --editbox $input 0 0 2>$output
returncode=$?

. ./report-edit
