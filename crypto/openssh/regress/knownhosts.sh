#	$OpenBSD: knownhosts.sh,v 1.1 2021/10/01 05:20:20 dtucker Exp $
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
