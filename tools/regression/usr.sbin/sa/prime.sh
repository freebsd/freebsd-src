#!/bin/sh
#
# Configure and run this script to create the files for regression testing
# for a new architecture/configuration.
#
# $FreeBSD$
#

# Set this to the path of the current sa command
SANEW=/usr/obj/usr/src/usr.sbin/sa/sa

# Set this to the path of the RELENG_6_2 sa
SA62=/usr/sbin/sa

# Machine architecture
ARCH=`uname -m`

# Location of lastcomm regression files
LCDIR=../../usr.bin/lastcomm

$SANEW -u $LCDIR/v1-$ARCH-acct.in >v1-$ARCH-u.out
$SANEW -u $LCDIR/v2-$ARCH-acct.in >v2-$ARCH-u.out
$SANEW -i $LCDIR/v1-$ARCH-acct.in >v1-$ARCH-sav.out
$SANEW -im $LCDIR/v1-$ARCH-acct.in >v1-$ARCH-usr.out
$SA62 -P v1-$ARCH-sav.in -U v1-$ARCH.usr $LCDIR/v1-$ARCH-acct.in >/dev/null
$SANEW -P v2-$ARCH-sav.in -U v2-$ARCH-usr.in $LCDIR/v1-$ARCH-acct.in >/dev/null
