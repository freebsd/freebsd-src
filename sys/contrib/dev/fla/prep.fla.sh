#!/bin/sh
# $FreeBSD: src/sys/contrib/dev/fla/prep.fla.sh,v 1.2 1999/09/08 12:42:15 phk Exp $

dev=fla0

grep "$dev.*sectors" /var/run/dmesg.boot | tr -d '(:)' | awk '
	{
	v = $3
	c = $5
	h = $7
	s = $9
	ss = c * h * s - s

	print "#",$0 > "_"
	print "g c"c" h"h" s"s > "_"
	print "p 1 165",s,ss > "_"
	print "a 1" > "_"

	print "#",$0  > "__"
	print "type: ESDI" > "__"
	print "disk:", $1 > "__"
	print "label:" > "__"
	print "flags:" > "__"
	print "bytes/sector: 512" > "__"
	print "sectors/track:", s > "__"
	print "tracks/cylinder:", h > "__"
	print "sectors/cylinder:", s * h > "__"
	print "cylinders:", c > "__"
	print "sectors/unit:", ss > "__"
	print "rpm: 3600" > "__"
	print "interleave: 1" > "__"
	print "trackskew: 0" > "__"
	print "cylinderskew: 0" > "__"
	print "headswitch: 0           # milliseconds" > "__"
	print "track-to-track seek: 0  # milliseconds" > "__"
	print "drivedata: 0 " > "__"
	print "8 partitions:" > "__"
	print "#        size   offset    fstype   [fsize bsize bps/cpg]" > "__"
	print "a:",ss,"0 4.2BSD 512 4096 " > "__"
	print "c:",ss,"0 unused 0 0" > "__"
	}
' 
fdisk -f _ -i -v $dev
disklabel -BrR ${dev} __
newfs /dev/r${dev}a
