#!/bin/sh
#
# makewhatis -- update the whatis database in the man directories.
#
# Copyright (c) 1990, 1991, John W. Eaton.
#
# You may distribute under the terms of the GNU General Public
# License as specified in the README file that comes with the man
# distribution.  
#
# John W. Eaton
# jwe@che.utexas.edu
# Department of Chemical Engineering
# The University of Texas at Austin
# Austin, Texas  78712

PATH=/bin:/usr/local/bin:/usr/ucb:/usr/bin

if [ $# = 0 ]
then
    echo "usage: makewhatis directory [...]"
    exit 1
fi
for dir in $*
do
    cd $dir
    for subdir in man*
    do
        if [ -d $subdir ]
        then
            for f in `find $subdir -type f -print`
            do
		suffix=`echo $f | sed -e 's/.*\\.//'`
		if [ ".$suffix" = "%compext%" ]; then
			output=%zcat%
		else
			output=cat
		fi
		$output $f | \
                sed -n '/^\.TH.*$/p
                	/^\.Dt.*$/p
                        /^\.S[hH][         ]*NAME/,/^\.S[hH]/p'|\
                sed -e 's/\\[   ]*\-/-/
                        s/^.P[Pp].*$//
                        s/\\(em//
                        s/\\fI//
                        s/\\fR//' |\
                awk 'BEGIN {insh = 0; inSh = 0; Nd = 0} {
                     if ($1 == ".TH" || $1 == ".Dt")
                       sect = $3
                     else if (($1 == ".br" && insh == 1)\
                      ||  ($1 == ".SH" && insh == 1)\
                      || ($1 == ".Sh" && inSh == 1)) {
                       if (i > 0 && nc > 0) {
                         for (k= 1; k <= nc; k++) {
				 namesect = sprintf("%s (%s)", name[k], sect)
				 printf("%s", namesect)
				 printf(" - ")
				 for (j = 0; j < i-1; j++)
				   printf("%s ", desc[j])
				 printf("%s\n", desc[i-1])
			 }
                       }
		       count = 0
		       i = 0
		       nc = 0
                     } else if ($1 == ".SH" && insh == 0) {
                       insh = 1
                       count = 0
                       i = 0
                       nc = 0
                     } else if ($1 == ".Sh" && inSh == 0) {
                       inSh = 1
                       i = 0
                       nc = 0
                     } else if (insh == 1) {
                       count++
                       if (count == 1 && NF > 2) {
			 start = 2
                         for (k = 1; k <= NF; k++)
                         	if ($k == "-") {
                         		start = k + 1
                         		break
				} else {
					sub(",","",$k)
					if ($k != "")
						name[++nc] = $k
				}
                         if (NF >= start)
                           for (j = start; j <= NF; j++)
                             desc[i++] = $j
                       } else {
                         for (j = 1; j <= NF; j++)
                           desc[i++] = $j
                       }
                     } else if ($1 == ".Nm" && inSh == 1 && Nd == 0) {
                         for (k = 2; k <= NF; k++) {
				sub(",","",$k)
				if ($k != "")
					name[++nc] = $k
			}
                     } else if ($1 == ".Nd" && inSh == 1) {
                     	Nd = 1
                     	for (j = 2; j <= NF; j++)
                     	   desc[i++] = $j
                     } else if (Nd == 1) {
                        start = 1
                        if ($1 ~ /\..*/)
                        	start = 2
                     	for (j = start; j <= NF; j++)
                     	   desc[i++] = $j
		     }
                }'
            done
        fi
    done | sort | colrm 80 | uniq > $dir/whatis.db.tmp
    mv $dir/whatis.db.tmp $dir/whatis
done

exit
