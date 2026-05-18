#-
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright 2026 Simon J Gerraty
#

atf_test_case safe_set_reject
safe_set_reject_head()
{
	atf_set "descr" "Verify that safe_set rejects shell meta chars"
}

safe_set_reject_body()
{
	__name="$(atf_get ident)"
	__input=$(mktemp -t "${__name}.input")

	cat <<'EOF' > "$__input"
: ignore=this
# ignore this too
# avoid # in the middle of a quoted value like:
# oops="this # will cause synatx error"
quoted="this and that"
simple=ok          # trailing comments ignored
  also=ok          # leading white-space ignored
	 also_wik=ok
host=`hostname`'   # backtics - delete line
os=$(uname -s)     # $() - delete line
oops=one;hostname' # replace ; with _ so: one_hostname
regex="prefix[abc-]*" # []* replaced with _
EOF

	__output=$(safe_set < "$__input" | tr '"\012' '\047;')
	atf_check_equal "$__output" "quoted='this and that';simple=ok;also=ok;also_wik=ok;oops=one_hostname_;regex='prefix_abc-__';"
}


atf_test_case safe_set_xtras
safe_set_xtras_head()
{
	atf_set "descr" "Verify that safe_set handles extra allowed chars"
}

safe_set_xtras_body()
{
	__name="$(atf_get ident)"
	__input=$(mktemp -t "${__name}.input")

	cat <<'EOF' > "$__input"
: ignore=this
# ignore this too
regex="prefix[abc-]*"
EOF

	__output=$(safe_set "[]*" < "$__input" | tr '"\012' '\047;')
	atf_check_equal "$__output" "regex='prefix[abc-]*';"
}

atf_init_test_cases()
{
	SAFE_EVAL=${SAFE_EVAL:-/libexec/safe_eval.sh}
	. $SAFE_EVAL
	atf_add_test_case safe_set_reject
	atf_add_test_case safe_set_xtras
}
