#!/bin/sh
# $Id: menubox9,v 1.8 2020/11/26 00:09:12 tom Exp $

. ./setup-vars

exec 3>&1
returntext=`$DIALOG --help-button \
	--clear \
	--title "Select Linux installation partition:" "$@" \
	--menu \
"Please select a partition from the following list to use for your \
root (/) Linux partition." 13 70 5 \
"/dev/hda2" "Linux native 30724312K" \
"/dev/hda4" "Linux native 506047K" \
"/dev/hdb1" "Linux native 4096543K" \
"/dev/hdb2" "Linux native 2586465K" \
"---" "(add none, continue with setup)" \
"---" "(add none, continue with setup)" \
"---" "(add none, continue with setup)" \
"---" "(add none, continue with setup)" \
"---" "(add none, continue with setup)" \
2>&1 1>&3`
returncode=$?
exec 3>&-

. ./report-string
