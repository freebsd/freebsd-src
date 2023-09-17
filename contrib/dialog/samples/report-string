#!/bin/sh
# $Id: report-string,v 1.8 2020/11/26 00:18:32 tom Exp $
# Report result passed in a string $returntext
# vile:shmode

case ${returncode:-0} in
  $DIALOG_OK)
    echo "Result is $returntext";;
  $DIALOG_CANCEL)
    echo "Cancel pressed.";;
  $DIALOG_HELP)
    echo "Help pressed ($returntext).";;
  $DIALOG_EXTRA)
    echo "Extra button pressed.";;
  $DIALOG_ITEM_HELP)
    echo "Item-help button pressed.";;
  $DIALOG_TIMEOUT)
    echo "Timeout expired.";;
  $DIALOG_ESC)
    if test -n "$returntext" ; then
      echo "$returntext"
    else
      echo "ESC pressed."
    fi
    ;;
  *)
    echo "Return code was $returncode";;
esac
