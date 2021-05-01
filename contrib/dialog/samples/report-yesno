#!/bin/sh
# $Id: report-yesno,v 1.7 2020/11/26 00:18:32 tom Exp $
# Report button-only, no $returntext
# vile:shmode

case ${returncode:-0} in
  $DIALOG_OK)
    echo "YES";;
  $DIALOG_CANCEL)
    echo "NO";;
  $DIALOG_HELP)
    echo "Help pressed.";;
  $DIALOG_EXTRA)
    echo "Extra button pressed.";;
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
