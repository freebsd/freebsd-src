#!/bin/sh
# usage: sh catman
# put the section numbers here:
SECTIONS="1 2 3 4 5 6 7 8"
MANDIR=/usr/share/man

formatman()
{
	suffix=`echo $1 | sed -e 's/.*\\.//'`
	(cd cat$section; rm -f $*)
	if [ ".$suffix" = "%compext%" ]; then
		adds=
		%zcat% man$section/$1 | nroff -man | %compress% > cat$section/$1$adds
	else
		adds=%compext%
		nroff -man < man$section/$1 | %compress% > cat$section/$1$adds
	fi
	echo "  "$* "->" $1$adds
	catfile=$1$adds; shift
	while [ $# -gt 0 ]
	do
		ln cat$section/$catfile cat$section/$1$adds
		shift
	done
}

cd $MANDIR
for section in $SECTIONS
do
  echo formatting section $section ...
  
  IFS=" "
  allfiles=`ls -i1 man$section | sort | awk '{if (inode ~ $1) printf "/" $2;
		 else printf " " $2; inode = $1 } END {printf "\n"}'` 
  for files in $allfiles
  do
    IFS="/"
    tfiles=`echo $files`
    IFS=" "
    formatman $tfiles
  done
done
exit 0
