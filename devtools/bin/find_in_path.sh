#! /bin/sh
#
# $Id: find_in_path.sh,v 8.2 1999-09-23 20:42:22 gshapiro Exp $
#
EX_OK=0
EX_NOT_FOUND=1

ifs="$IFS"; IFS="${IFS}:"
for dir in $PATH /usr/5bin /usr/ccs/bin
do
	if [ -r $dir/$1 ]
	then
		echo $dir/$1
		exit $EX_OK
	fi
done
IFS=$ifs

exit $EX_NOT_FOUND
