#	$OpenBSD: broken-pipe.sh,v 1.4 2002/03/15 13:08:56 markus Exp $
#	Placed in the Public Domain.

tid="broken pipe test"

for p in 1 2; do
	trace "protocol $p"
	for i in 1 2 3 4; do
		${SSH} -$p -F $OBJ/ssh_config_config nexthost echo $i 2> /dev/null | true
		r=$?
		if [ $r -ne 0 ]; then
			fail "broken pipe returns $r for protocol $p"
		fi
	done
done
