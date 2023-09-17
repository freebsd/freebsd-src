#!/bin/sh
# $Id: menubox3,v 1.10 2020/11/26 00:03:58 tom Exp $

. ./setup-vars

. ./setup-tempfile

$DIALOG --clear --item-help --title "MENU BOX" "$@" \
        --menu "Hi, this is a menu box. You can use this to \n\
present a list of choices for the user to \n\
choose. If there are more items than can fit \n\
on the screen, the menu will be scrolled. \n\
You can use the UP/DOWN arrow keys, the first \n\
letter of the choice as a hot key, or the \n\
number keys 1-9 to choose an option.\n\
Try it now!\n\n\
          Choose the OS you like:" 20 51 4 \
        "Linux"  "The Great Unix Clone for 386/486"    "Why use Linux?" \
        "NetBSD" "Another free Unix Clone for 386/486" "Or NetBSD?" \
        "OS/2"   "IBM OS/2"                            "aka \"Warp\"" \
        "WIN NT" "Microsoft Windows NT"                "hmm" \
        "PC-DOS"  "IBM PC DOS"                         "clone of a clone" \
        "MS-DOS"  "Microsoft DOS"                      "DOS: Disk Operating System, originally for an IBM contract, hence using the same jargon" 2> $tempfile

returncode=$?

. ./report-tempfile
