#	$OpenBSD: envpass.sh,v 1.1 2004/04/27 09:47:30 djm Exp $
#	Placed in the Public Domain.

tid="environment passing"

# NB accepted env vars are in test-exec.sh (_XXX_TEST_* and _XXX_TEST)

trace "pass env, don't accept"
verbose "test $tid: pass env, don't accept"
_TEST_ENV=blah ${SSH} -oSendEnv="*" -F $OBJ/ssh_proxy otherhost \
	'[ -z "$_TEST_ENV" ]'
r=$?
if [ $r -ne 0 ]; then
	fail "environment found"
fi

trace "don't pass env, accept"
verbose "test $tid: don't pass env, accept"
${SSH} -F $OBJ/ssh_proxy otherhost \
	'[ -z "$_XXX_TEST_A" -a -z "$_XXX_TEST_B" ]'
r=$?
if [ $r -ne 0 ]; then
	fail "environment found"
fi

trace "pass single env, accept single env"
verbose "test $tid: pass single env, accept single env"
_XXX_TEST=blah ${SSH} -oSendEnv="_XXX_TEST" -F $OBJ/ssh_proxy otherhost \
	'[ "x$_XXX_TEST" = "xblah" ]'
r=$?
if [ $r -ne 0 ]; then
	fail "environment not found"
fi

trace "pass multiple env, accept multiple env"
verbose "test $tid: pass multiple env, accept multiple env"
_XXX_TEST_A=1 _XXX_TEST_B=2 ${SSH} -oSendEnv="_XXX_TEST_*" \
    -F $OBJ/ssh_proxy otherhost \
	'[ "x$_XXX_TEST_A" = "x1" -a "x$_XXX_TEST_B" = "x2" ]'
r=$?
if [ $r -ne 0 ]; then
	fail "environment not found"
fi

