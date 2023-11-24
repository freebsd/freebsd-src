#	$OpenBSD: knownhosts.sh,v 1.2 2023/02/09 09:55:33 dtucker Exp $
#	Placed in the Public Domain.

tid="known hosts"

opts="-F $OBJ/ssh_proxy"

trace "test initial connection"
${SSH} $opts somehost true || fail "initial connection"

trace "learn hashed known host"
>$OBJ/known_hosts
${SSH} -ohashknownhosts=yes -o stricthostkeychecking=no $opts somehost true \
   || fail "learn hashed known_hosts"

trace "test hashed known hosts"
${SSH} $opts somehost true || fail "reconnect with hashed known hosts"

trace "no newline at end of known_hosts"
printf "something" >$OBJ/known_hosts
${SSH} $opts -ostricthostkeychecking=no somehost true \
    || fail "hostkey update, missing newline, no strict"
${SSH} $opts -ostricthostkeychecking=yes somehost true \
    || fail "reconnect after adding with missing newline"

trace "newline at end of known_hosts"
printf "something\n" >$OBJ/known_hosts
${SSH} $opts -ostricthostkeychecking=no somehost true \
    || fail "hostkey update, newline, no strict"
${SSH} $opts -ostricthostkeychecking=yes somehost true \
    || fail "reconnect after adding without missing newline"
lines=`wc -l <$OBJ/known_hosts`
if [ $lines -ne 2 ]; then
	fail "expected 2 lines in known_hosts, found $lines"
fi
