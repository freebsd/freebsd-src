#!/bin/sh
# $Id: password1,v 1.7 2020/11/26 00:03:58 tom Exp $

. ./setup-vars

. ./setup-tempfile

$DIALOG --title "INPUT BOX" --clear \
	--insecure "$@" \
        --passwordbox "Hi, this is an password dialog box. You can use \n
this to ask questions that require the user \n
to input a string as the answer. You can \n
input strings of length longer than the \n
width of the input box, in that case, the \n
input field will be automatically scrolled. \n
You can use BACKSPACE to correct errors. \n\n
Try entering your name below:" 16 51 2> $tempfile

returncode=$?

. ./report-tempfile
