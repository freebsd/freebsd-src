#!/bin/sh
# $Id: programbox2,v 1.4 2020/11/26 00:05:11 tom Exp $

. ./setup-vars

. ./setup-tempfile

ls -1 >$tempfile
(
while true
do
read text
test -z "$text" && break
ls -ld "$text" || break
sleep 0.1
done <$tempfile
) |

$DIALOG --title "PROGRAMBOX" "$@" --programbox "ProgramBox" 20 70

returncode=$?
. ./report-button
