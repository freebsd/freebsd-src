#!/bin/sh
#
# Author: Luke Kendall
#
MYNAME=`basename $0`
usage="usage: $MYNAME [man-directory]
	(generates permuted index of -man files in directory)"
md=/usr/man
#
if [ $# = 0 ]
then
    echo "$MYNAME: no man directory specified: assuming $md"
elif [ $# != 1 ]
then
    echo "$usage"
    exit 1
elif [ -d $1 ]
then
    md="$1"
else
    echo "$usage"
    exit 1
fi
echo "Permuted index of $md:"
out=ptx.tr
# ------ clumsy permuted index macros (replaced by stuff below) ------------
cat <<'EOF' > $out
.pn 1
.de xx
\\$1 \\$2  \\fB\\$3\\fR  \\$4	\\s-1\\$5\\s0
..
.pl 10i
.de NP
.ev 1
.ft 1
.ps 10
.sp 0.75c
.tl '\s-2\\fIpermuted index\\fP\s0'\- \\n% \-'\s-2\\fIpermuted index\\fP\s0'
.pn +1
.bp
.ev
..
.wh 9i NP
.nf
.na
.ta 6.5i-1.1iR 6.5iR 6.51iR 6.52R
.ll 6.0i
.po 0i
.sp 0.25i
'\"
EOF
# ------  -------  -------  -------  -------  -------
# ------ alternate permuted index macros  (from net) ------------
cat <<'EOF' > $out
.pl 10i
.de NP
.ev 1
.ft 1
.ps 10
.sp 0.75c
.tl '\s-2\\fIpermuted index\\fP\s0'\- \\n% \-'\s-2\\fIpermuted index\\fP\s0'
.pn +1
.bp
.ev
..
.wh 9i NP
.po 0.5i
.sp 0.25i
.tr ~               \" tildes will translate to blanks
'\".ll 80              \" line length of output
.ll 6.0i	    \" line length of output
.nf                 \" must be in no-fill mode
.nr )r \n(.lu-10n   \" set position of reference in line (10 less than length)
.nr )k \n()ru/2u    \" set position of keyword (approx. centered)
.ds s2 ~~~          \" this is the center gap -- 3 spaces
.de xx              \"definition of xx macro
.ds s1\"            \" initialise to null string
.if \w@\\$2@ .ds s1 ~\"       \"set to single blank if there is second arg
.ds s3\"                      \" initialise to null string
.if \w@\\$4@ .ds s3 ~\"       \"set to single blank if there is second arg
.ds s4 ~\"                    \" set to single blank
.ds s5 ~\"                    \" set to single blank
.ds y \\*(s4\a\\*(s5\"        \" blank, leader, blank
.ta \\n()ru-\w@\\*(s5@u       \" set tab just to left of ref
\h@\\n()ku-\w@\\$1\\*(s1\\$2\\*(s2@u@\\$1\\*(s1\\$2\\*(s2\\$3\\*(s3\\$4\\*y\\$5
..
 ~
EOF
# ------  -------  -------  -------  -------  -------
find $md -type f -name "*.[1-8nl]*" -print |
while read f
do
    man=`basename $f`
    man=`expr "$man" : "\(.*\)\.[^\.]*"`
echo $man:
    #
    # Use 1st non-"." and non-"'" started line as input to ptx (this
    # should be the synopsis after the `.SH NAME');
    # strip any "\-" from it (a silly sort key for ptx to avoid);
    # insert a leading man page name for the -r option to find
    #
    sed -n '/^[^.]/s/\\-//g;/^[^.]/p;/^[^.]/q' $f | sed "s/^/($man) /"
done  | ptx -t -f -r  >> $out
#
# Turn the troff'able permuted index file into PostScript
#
psroff -t -rL10i $out > ptx.ps
echo "$out and ptx.ps produced from man directory $md."
