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
            for f in `find . -name '*' -print`
            do
                sed -n '/^\.TH.*$/p
                        /^\.SH[         ]*NAME/,/^\.SH/p' $f |\
                sed -e 's/\\[   ]*\-/-/
                        s/^.PP.*$//
                        s/\\(em//
                        s/\\fI//
                        s/\\fR//' |\
                awk 'BEGIN {insh = 0} {
                     if ($1 == ".TH")
                       sect = $3
                     else if ($1 == ".SH" && insh == 1) {
                       if (i > 0 && name != NULL) {
                         namesect = sprintf("%s (%s)", name, sect)
                         printf("%-20.20s", namesect)
                         printf(" - ")
                         for (j = 0; j < i-1; j++)
                           printf("%s ", desc[j])
                         printf("%s\n", desc[i-1])
                       }
                     } else if ($1 == ".SH" && insh == 0) {
                       insh = 1
                       count = 0
                       i = 0
                     } else if (insh == 1) {
                       count++
                       if (count == 1 && NF > 2) {
                         start = 2
                         if ($2 == "-") start = 3
                         if (NF > start + 1)
                           for (j = start; j <= NF; j++)
                             desc[i++] = $j
                           name = $1
                       } else {
                         for (j = 1; j <= NF; j++)
                           desc[i++] = $j
                       }
                     }
                }'
            done
            cd ..
        fi
    done | sort | colrm 80 > $dir/whatis.db.tmp
    mv $dir/whatis.db.tmp $dir/whatis
done

exit
