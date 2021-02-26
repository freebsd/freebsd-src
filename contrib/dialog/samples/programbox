#!/bin/sh
# $Id: programbox,v 1.4 2020/11/26 00:05:11 tom Exp $

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

$DIALOG --title "PROGRAMBOX" "$@" --programbox 20 70

returncode=$?
. ./report-button
