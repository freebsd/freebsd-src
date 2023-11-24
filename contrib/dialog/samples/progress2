#!/bin/sh
# $Id: progress2,v 1.8 2020/11/26 00:05:11 tom Exp $

. ./setup-vars

. ./setup-tempfile

ls -1 >$tempfile
(
while true
do
read text
test -z "$text" && break
ls -ld "$text" || break
sleep 1
done <$tempfile
) |

$DIALOG --title "PROGRESS" "$@" --progressbox "This is a detailed description\nof the progress-box." 20 70

returncode=$?
. ./report-button
