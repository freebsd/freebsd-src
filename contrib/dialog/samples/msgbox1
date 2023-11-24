#!/bin/sh
# $Id: msgbox1,v 1.11 2019/12/11 01:56:43 tom Exp $

. ./setup-vars

$DIALOG --title "MESSAGE BOX" --clear "$@" \
        --msgbox "Hi, this is a simple message box. You can use this to 
                  display any message you like. The box will remain until
                  you press the ENTER key.  This box is being displayed 
                  with dialogs default aspect ratio of 9." 0 0
test $? = "$DIALOG_ESC" && exit

$DIALOG --aspect 12 --title "MESSAGE BOX  aspect=12" --clear "$@" \
        --msgbox "Hi, this is a simple message box. You can use this to 
                  display any message you like. The box will remain until
                  you press the ENTER key.  This box is being displayed 
                  with an aspect ratio of 12." 0 0
test $? = "$DIALOG_ESC" && exit

$DIALOG --aspect 6 --title "MESSAGE BOX  aspect=6" --clear "$@" \
        --msgbox "Hi, this is a simple message box. You can use this to 
                  display any message you like. The box will remain until
                  you press the ENTER key.  This box is being displayed 
                  with an aspect ratio of 6." 0 0
test $? = "$DIALOG_ESC" && exit

$DIALOG --aspect 6 --cr-wrap --title "MESSAGE BOX aspect=6 with --cr-wrap" \
        --clear "$@" --msgbox " \
           Hi, this is a simple
           message box. You can
          use this to display any 
             message you like.
The box will remain until you press the ENTER key.  This box is being displayed with an aspect ratio of 6, and using --cr-wrap.\n" 0 0
test $? = "$DIALOG_ESC" && exit
