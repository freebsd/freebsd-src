#!/bin/sh
# $Id: programbox3,v 1.2 2020/11/26 00:05:11 tom Exp $

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

$DIALOG --title "PROGRAMBOX" "$@" --programbox -1 -1

returncode=$?
. ./report-button
