#! /bin/sh
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
*/*) cd `expr X$0 : 'X\(.*\)/'` ;;
esac

echo "Extracting makedepend (with variable substitutions)"
rm -f makedepend
$spitshell >makedepend <<!GROK!THIS!
$startsh
# makedepend.SH
#
MAKE=$make
trnl='$trnl'
!GROK!THIS!
$spitshell >>makedepend <<'!NO!SUBS!'

# This script should be called with 
#     sh ./makedepend MAKE=$(MAKE)
case "$1" in 
	MAKE=*) eval $1 ;;
esac

export PATH || (echo "OOPS, this isn't sh.  Desperation time.  I will feed myself to sh."; sh \$0; kill \$\$)

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

# We need .. when we are in the x2p directory if we are using the
# cppstdin wrapper script.
# Put .. and . first so that we pick up the present cppstdin, not
# an older one lying about in /usr/local/bin.
PATH=".$path_sep..$path_sep$PATH"
export PATH

$cat /dev/null >.deptmp
$rm -f *.c.c c/*.c.c
if test -f Makefile; then
    rm -f $firstmakefile
    cp Makefile $firstmakefile
    # On QNX, 'cp' preserves timestamp, so $firstmakefile appears
    # to be out of date.  I don't know if OS/2 has touch, so do this:
    case "$osname" in
    os2) ;;
    netbsd) ;;
    *) $touch $firstmakefile ;;
    esac
fi
mf=$firstmakefile
if test -f $mf; then
    defrule=`<$mf sed -n		\
	-e '/^\.c\$(OBJ_EXT):.*;/{'	\
	-e    's/\$\*\.c//'		\
	-e    's/^[^;]*;[	 ]*//p'	\
	-e    q				\
	-e '}'				\
	-e '/^\.c\$(OBJ_EXT): *$/{'	\
	-e    N				\
	-e    's/\$\*\.c//'		\
	-e    's/^.*\n[	 ]*//p'		\
	-e    q				\
	-e '}'`
fi
case "$defrule" in
'') defrule='$(CC) -c $(CFLAGS)' ;;
esac

: Create files in UU directory to avoid problems with long filenames
: on systems with 14 character filename limits so file.c.c and file.c
: might be identical
$test -d UU || mkdir UU

$MAKE clist || ($echo "Searching for .c files..."; \
	$echo *.c | $tr ' ' $trnl | $egrep -v '\*' >.clist)
for file in `$cat .clist`; do
# for file in `cat /dev/null`; do
	if [ "$osname" = uwin ]; then
		uwinfix="-e s,\\\\\\\\,/,g -e s,\\([a-zA-Z]\\):/,/\\1/,g"
	else
		if [ "$osname" = os2 ]; then
			uwinfix="-e s,\\\\\\\\,/,g"
		else
			if [ "$archname" = cygwin ]; then
				uwinfix="-e s,\\\\\\\\,/,g"
			else
				if [ "$osname" = posix-bc ]; then
					uwinfix="-e s/\\*POSIX(\\(.*\\))/\\1/"
				else
					uwinfix=
				fi
			fi
		fi
	fi
    case "$file" in
    *.c) filebase=`basename $file .c` ;;
    *.y) filebase=`basename $file .y` ;;
    esac
    case "$file" in
    */*) finc="-I`echo $file | sed 's#/[^/]*$##`" ;;
    *)   finc= ;;
    esac
    $echo "Finding dependencies for $filebase$_o."
    ( $echo "#line 1 \"$file\""; \
      $sed -n <$file \
	-e "/^${filebase}_init(/q" \
	-e '/^#line/d' \
	-e '/^#/{' \
	-e 's|/\*.*$||' \
	-e 's|\\$||' \
	-e p \
	-e '}' ) >UU/$file.c
    if [ "$osname" = os390 ]; then
        if [ "$file" = perly.c ]; then
            $echo '#endif' >>UU/$file.c
        fi
        $cppstdin $finc -I. $cppflags $cppminus <UU/$file.c |
        $sed \
    	    -e '/^#.*<stdin>/d' \
	    -e '/^#.*"-"/d' \
	    -e 's#\.[0-9][0-9]*\.c#'"$file.c#" \
	    -e 's/^[	 ]*#[	 ]*line/#/' \
	    -e '/^# *[0-9][0-9]* *[".\/]/!d' \
	    -e 's/^.*"\(.*\)".*$/'$filebase'\$(OBJ_EXT): \1/' \
	    -e 's/^# *[0-9][0-9]* \(.*\)$/'$filebase'\$(OBJ_EXT): \1/' \
	    -e 's|: \./|: |' \
	    -e 's|\.c\.c|.c|' $uwinfix | \
        $uniq | $sort | $uniq >> .deptmp
    else
        $cppstdin $finc -I. $cppflags $cppminus <UU/$file.c |
        $sed \
	    -e '1d' \
	    -e '/^#.*<stdin>/d' \
	    -e '/^#.*"-"/d' \
	    -e 's#\.[0-9][0-9]*\.c#'"$file.c#" \
	    -e 's/^[	 ]*#[	 ]*line/#/' \
	    -e '/^# *[0-9][0-9]* *[".\/]/!d' \
	    -e 's/^.*"\(.*\)".*$/'$filebase'\$(OBJ_EXT): \1/' \
	    -e 's/^# *[0-9][0-9]* \(.*\)$/'$filebase'\$(OBJ_EXT): \1/' \
	    -e 's|: \./|: |' \
	    -e 's|\.c\.c|.c|' $uwinfix | \
        $uniq | $sort | $uniq >> .deptmp
    fi
done

$sed <$mf >$mf.new -e '1,/^# AUTOMATICALLY/!d'

$MAKE shlist || ($echo "Searching for .SH files..."; \
	$echo *.SH | $tr ' ' $trnl | $egrep -v '\*' >.shlist)

# Now extract the dependencies on makedepend.SH and Makefile.SH
# (they should reside in the main Makefile):
rm -f .shlist.old
mv .shlist .shlist.old
$egrep -v '^makedepend\.SH' <.shlist.old >.shlist
rm -f .shlist.old
mv .shlist .shlist.old
$egrep -v '^Makefile\.SH' <.shlist.old >.shlist
rm -f .shlist.old
mv .shlist .shlist.old
$egrep -v '^perl_exp\.SH' <.shlist.old >.shlist
rm -f .shlist.old
mv .shlist .shlist.old
$egrep -v '^config_h\.SH' <.shlist.old >.shlist
rm .shlist.old

if $test -s .deptmp; then
    for file in `cat .shlist`; do
	$echo `$expr X$file : 'X\(.*\).SH'`: $file $TOP/config.sh \; \
	    $sh $file >> .deptmp
    done
    $echo "Updating $mf..."
    $echo "# If this runs make out of memory, delete /usr/include lines." \
	>> $mf.new
    $sed 's|^\(.*\$(OBJ_EXT):\) *\(.*/.*\.c\) *$|\1 \2; '"$defrule \2|" .deptmp \
       >>$mf.new
else
    $MAKE hlist || ($echo "Searching for .h files..."; \
	$echo *.h | $tr ' ' $trnl | $egrep -v '\*' >.hlist)
    $echo "You don't seem to have a proper C preprocessor.  Using grep instead."
    $egrep '^#include ' `cat .clist` `cat .hlist`  >.deptmp
    $echo "Updating $mf..."
    <.clist $sed -n							\
	-e '/\//{'							\
	-e   's|^\(.*\)/\(.*\)\.c|\2\$(OBJ_EXT): \1/\2.c; '"$defrule \1/\2.c|p"	\
	-e   d								\
	-e '}'								\
	-e 's|^\(.*\)\.c|\1\$(OBJ_EXT): \1.c|p' >> $mf.new
    <.hlist $sed -n 's|\(.*/\)\(.*\)|s= \2= \1\2=|p' >.hsed
    <.deptmp $sed -n 's|c:#include "\(.*\)".*$|o: \1|p' | \
       $sed 's|^[^;]*/||' | \
       $sed -f .hsed >> $mf.new
    <.deptmp $sed -n 's|h:#include "\(.*\)".*$|h: \1|p' | \
       $sed -f .hsed >> $mf.new
    for file in `$cat .shlist`; do
	$echo `$expr X$file : 'X\(.*\).SH'`: $file $TOP/config.sh \; \
	    $sh $file >> $mf.new
    done
fi
$rm -f $mf.old
$cp $mf $mf.old
$rm -f $mf
$cp $mf.new $mf
$rm $mf.new
$echo "# WARNING: Put nothing here or make depend will gobble it up!" >> $mf
$rm -rf .deptmp UU .shlist .clist .hlist .hsed

!NO!SUBS!
$eunicefix makedepend
chmod +x makedepend
case `pwd` in
*SH)
    $rm -f ../makedepend
    ln makedepend ../makedepend
    ;;
esac
