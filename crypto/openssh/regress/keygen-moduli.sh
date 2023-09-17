#	$OpenBSD: keygen-moduli.sh,v 1.4 2020/01/02 13:25:38 dtucker Exp $
#	Placed in the Public Domain.

tid="keygen moduli"

dhgex=0
for kex in `${SSH} -Q kex`; do
	case $kex in
		diffie-hellman-group*)	dhgex=1 ;;
	esac
done

# Try "start at the beginning and stop after 1", "skip 1 then stop after 1"
# and "skip 2 and run to the end with checkpointing".  Since our test data
# file has 3 lines, these should always result in 1 line of output.
if [ "x$dhgex" = "x1" ]; then
    for i in "-O lines=1" "-O start-line=1 -O lines=1" "-O start-line=2 -O checkpoint=$OBJ/moduli.ckpt"; do
	trace "keygen $i"
	rm -f $OBJ/moduli.out $OBJ/moduli.ckpt
	${SSHKEYGEN} -M screen -f ${SRC}/moduli.in $i $OBJ/moduli.out 2>/dev/null || \
	    fail "keygen screen failed $i"
	lines=`wc -l <$OBJ/moduli.out`
	test "$lines" -eq "1" || fail "expected 1 line, got $lines"
    done
fi

rm -f $OBJ/moduli.out $OBJ/moduli.ckpt
