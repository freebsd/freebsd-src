#!/bin/sh
#
# Validate all locales installed in specified directory
# (by default check system locales)
#
# $FreeBSD$
#

LOCALEDIR=/usr/share/locale

if [ "$1" != "" ]; then
	LOCALEDIR=$1
fi

if [ ! -x ./localeck ]; then
	echo "ERROR: build test program first."
	exit 1
fi

PATH_LOCALE=$LOCALEDIR
LOCALES=0
ERRORS=0

echo "Validating locales in $LOCALEDIR" 
echo

for i in `ls -1 $LOCALEDIR`
do
	LOCALES=`expr $LOCALES + 1`
	./localeck $i
	if [ $? != 0 ]; then
		ERRORS=`expr $ERRORS + 1`
	fi
done

echo
echo "Validation test complete."
echo "$LOCALES locale(s) were checked"
echo "$ERRORS invalid locale(s) were found"

