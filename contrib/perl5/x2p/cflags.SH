case $CONFIGDOTSH in
'')
	if test -f config.sh; then TOP=.;
	elif test -f ../config.sh; then TOP=..;
	elif test -f ../../config.sh; then TOP=../..;
	elif test -f ../../../config.sh; then TOP=../../..;
	elif test -f ../../../../config.sh; then TOP=../../../..;
	else
		echo "Can't find config.sh."; exit 1
	fi
	. $TOP/config.sh
	;;
esac
: This forces SH files to create target in same directory as SH file.
: This is so that make depend always knows where to find SH derivatives.
case "$0" in
*/cflags.SH) cd `expr X$0 : 'X\(.*\)/'` ;;
cflags.SH) ;;
*) case `pwd` in
   */x2p) ;;
   *) if test -d x2p; then cd x2p
      else echo "Can't figure out where to write output."; exit 1
	  fi;;
   esac;;
esac
echo "Extracting x2p/cflags (with variable substitutions)"
: This section of the file will have variable substitutions done on it.
: Move anything that needs config subs from !NO!SUBS! section to !GROK!THIS!.
: Protect any dollar signs and backticks that you do not want interpreted
: by putting a backslash in front.  You may delete these comments.
rm -f cflags
$spitshell >cflags <<!GROK!THIS!
!GROK!THIS!

: In the following dollars and backticks do not need the extra backslash.
$spitshell >>cflags <<'!NO!SUBS!'
case $CONFIG in
'')
	if test -f config.sh; then TOP=.;
	elif test -f ../config.sh; then TOP=..;
	elif test -f ../../config.sh; then TOP=../..;
	elif test -f ../../../config.sh; then TOP=../../..;
	elif test -f ../../../../config.sh; then TOP=../../../..;
	else
		echo "Can't find config.sh."; exit 1
	fi
	. $TOP/config.sh
	;;
esac

also=': '
case $# in
1) also='echo 1>&2 "	  CCCMD = "'
esac

case $# in
0) set *.c; echo "The current C flags are:" ;;
esac

set `echo "$* " | sed -e 's/\.[oc] / /g' -e 's/\.obj / /g' -e "s/\\$obj_ext / /g"`

for file do

    case "$#" in
    1) ;;
    *) echo $n "    $file.c	$c" ;;
    esac

    : allow variables like str_cflags to be evaluated

    eval 'eval ${'"${file}_cflags"'-""}'

    : or customize here

    case "$file" in
    a2p) ;;
    a2py) ;;
    hash) ;;
    str) ;;
    util) ;;
    walk) ;;
    *) ;;
    esac

    ccflags="`echo $ccflags | sed -e 's/-DMULTIPLICITY//'`"

    echo "$cc -c $ccflags $optimize"
    eval "$also "'"$cc -c $ccflags $optimize"'

    . $TOP/config.sh

done
!NO!SUBS!
chmod 755 cflags
$eunicefix cflags
