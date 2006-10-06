#	$OpenBSD: forwarding.sh,v 1.6 2006/07/11 18:51:21 markus Exp $
#	Placed in the Public Domain.

tid="local and remote forwarding"
DATA=/bin/ls${EXEEXT}

start_sshd

base=33
last=$PORT
fwd=""
for j in 0 1 2; do
	for i in 0 1 2; do
		a=$base$j$i
		b=`expr $a + 50`
		c=$last
		# fwd chain: $a -> $b -> $c
		fwd="$fwd -L$a:127.0.0.1:$b -R$b:127.0.0.1:$c"
		last=$a
	done
done
for p in 1 2; do
	q=`expr 3 - $p`
	trace "start forwarding, fork to background"
	${SSH} -$p -F $OBJ/ssh_config -f $fwd somehost sleep 10

	trace "transfer over forwarded channels and check result"
	${SSH} -$q -F $OBJ/ssh_config -p$last -o 'ConnectionAttempts=4' \
		somehost cat $DATA > $OBJ/ls.copy
	test -f $OBJ/ls.copy			|| fail "failed copy $DATA"
	cmp $DATA $OBJ/ls.copy			|| fail "corrupted copy of $DATA"

	sleep 10
done

for p in 1 2; do
for d in L R; do
	trace "exit on -$d forward failure, proto $p"

	# this one should succeed
	${SSH} -$p -F $OBJ/ssh_config \
	    -$d ${base}01:127.0.0.1:$PORT \
	    -$d ${base}02:127.0.0.1:$PORT \
	    -$d ${base}03:127.0.0.1:$PORT \
	    -$d ${base}04:127.0.0.1:$PORT \
	    -oExitOnForwardFailure=yes somehost true
	if [ $? != 0 ]; then
		fail "connection failed, should not"
	else
		# this one should fail
		${SSH} -q -$p -F $OBJ/ssh_config \
		    -$d ${base}01:127.0.0.1:$PORT \
		    -$d ${base}02:127.0.0.1:$PORT \
		    -$d ${base}03:127.0.0.1:$PORT \
		    -$d ${base}01:127.0.0.1:$PORT \
		    -$d ${base}04:127.0.0.1:$PORT \
		    -oExitOnForwardFailure=yes somehost true
		r=$?
		if [ $r != 255 ]; then
			fail "connection not termintated, but should ($r)"
		fi
	fi
done
done

for p in 1 2; do
	trace "simple clear forwarding proto $p"
	${SSH} -$p -F $OBJ/ssh_config -oClearAllForwardings=yes somehost true

	trace "clear local forward proto $p"
	${SSH} -$p -f -F $OBJ/ssh_config -L ${base}01:127.0.0.1:$PORT \
	    -oClearAllForwardings=yes somehost sleep 10
	if [ $? != 0 ]; then
		fail "connection failed with cleared local forwarding"
	else
		# this one should fail
		${SSH} -$p -F $OBJ/ssh_config -p ${base}01 true \
		     2>${TEST_SSH_LOGFILE} && \
			fail "local forwarding not cleared"
	fi
	sleep 10
	
	trace "clear remote forward proto $p"
	${SSH} -$p -f -F $OBJ/ssh_config -R ${base}01:127.0.0.1:$PORT \
	    -oClearAllForwardings=yes somehost sleep 10
	if [ $? != 0 ]; then
		fail "connection failed with cleared remote forwarding"
	else
		# this one should fail
		${SSH} -$p -F $OBJ/ssh_config -p ${base}01 true \
		     2>${TEST_SSH_LOGFILE} && \
			fail "remote forwarding not cleared"
	fi
	sleep 10
done
