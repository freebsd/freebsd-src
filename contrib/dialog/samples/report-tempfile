#!/bin/sh
# $Id: report-tempfile,v 1.9 2020/11/26 00:18:32 tom Exp $
# Report results in a temporary-file.
# vile:shmode

case "${returncode:-0}" in
  $DIALOG_OK)
    echo "Result: `cat "$tempfile"`";;
  $DIALOG_CANCEL)
    echo "Cancel pressed.";;
  $DIALOG_HELP)
    echo "Help pressed: `cat "$tempfile"`";;
  $DIALOG_EXTRA)
    echo "Extra button pressed.";;
  $DIALOG_ITEM_HELP)
    echo "Item-help button pressed: `cat "$tempfile"`";;
  $DIALOG_TIMEOUT)
    echo "Timeout expired.";;
  $DIALOG_ESC)
    if test -s "$tempfile" ; then
      cat "$tempfile"
    else
      echo "ESC pressed."
    fi
    ;;
  *)
    echo "Return code was $returncode";;
esac
