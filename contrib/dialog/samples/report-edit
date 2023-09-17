#!/bin/sh
# $Id: report-edit,v 1.8 2020/11/26 00:18:32 tom Exp $
# Report results from editing.
# vile:shmode

case ${returncode:-0} in
  $DIALOG_OK)
    diff -c "${input:-input}" "${output:-output}"
    echo "OK"
    ;;
  $DIALOG_CANCEL)
    echo "Cancel pressed";;
  $DIALOG_HELP)
    echo "Help pressed";;
  $DIALOG_EXTRA)
    echo "Extra pressed";;
  $DIALOG_ITEM_HELP)
    echo "Item-help button pressed.";;
  $DIALOG_TIMEOUT)
    echo "Timeout expired.";;
  $DIALOG_ERROR)
    echo "ERROR!";;
  $DIALOG_ESC)
    echo "ESC pressed.";;
  *)
    echo "Return code was $returncode";;
esac
