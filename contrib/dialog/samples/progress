#!/bin/sh
# $Id: progress,v 1.8 2020/11/26 00:05:11 tom Exp $

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

$DIALOG --title "PROGRESS" "$@" --progressbox 20 70

returncode=$?
. ./report-button
