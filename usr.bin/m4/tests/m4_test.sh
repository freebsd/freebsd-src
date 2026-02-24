#
# Copyright (c) 2026 Klara, Inc.
#
# SPDX-License-Identifier: BSD-2-Clause
#

m4_test()
{
	local dir=$(atf_get_srcdir)
	local rc=0
	local args opt output script
	while getopts "1gP" opt ; do
		case ${opt} in
		1)
			rc=1
			;;
		*)
			args="${args% }-${opt}"
			;;
		esac
	done
	shift $((OPTIND - 1))
	script=$1
	output=$2
	if [ -z "${output}" ] ; then
		output="${script}"
	fi
	if [ -f "${dir}/regress.${output}.out" ] ; then
		ln -s "${dir}/regress.${output}.out" out
	else
		atf_fail "regress.${output}.out not found"
	fi
	if [ -f "${dir}/regress.${output}.err" ] ; then
		ln -s "${dir}/regress.${output}.err" err
	else
		touch err
	fi
	if [ -f "${dir}/${script}.m4.uu" ] ; then
		atf_check uudecode -o "${script}.m4" "${dir}/${script}.m4.uu"
	elif [ -f "${dir}/${script}.m4" ] ; then
		ln -s "${dir}/${script}.m4" "${script}.m4"
	else
		atf_fail "${script}.m4 not found"
	fi
	atf_check -s exit:${rc} -o file:out -e file:err \
	    m4 -I "${dir}" ${args} "${script}.m4"
}

args_head()
{
}
args_body()
{
	m4_test args
}

args2_head()
{
}
args2_body()
{
	m4_test args2
}

comments_head()
{
}
comments_body()
{
	m4_test comments
}

defn_head()
{
}
defn_body()
{
	m4_test defn
}

esyscmd_head()
{
}
esyscmd_body()
{
	m4_test esyscmd
}

eval_head()
{
}
eval_body()
{
	m4_test eval
}

ff_after_dnl_head()
{
}
ff_after_dnl_body()
{
	m4_test ff_after_dnl
}

gnueval_head()
{
}
gnueval_body()
{
	m4_test -g gnueval
}

gnuformat_head()
{
}
gnuformat_body()
{
	m4_test -g gnuformat
}

gnupatterns_head()
{
}
gnupatterns_body()
{
	m4_test -g gnupatterns
}

gnupatterns2_head()
{
}
gnupatterns2_body()
{
	m4_test -g gnupatterns2
}

gnuprefix_head()
{
}
gnuprefix_body()
{
	m4_test -P gnuprefix
}

gnusofterror_head()
{
}
gnusofterror_body()
{
	m4_test -1 -g gnusofterror
}

gnutranslit2_head()
{
}
gnutranslit2_body()
{
	m4_test -g translit2 gnutranslit2
}

includes_head()
{
}
includes_body()
{
	m4_test includes
}

m4wrap3_head()
{
}
m4wrap3_body()
{
	m4_test m4wrap3
}

patterns_head()
{
}
patterns_body()
{
	m4_test patterns
}

quotes_head()
{
}
quotes_body()
{
	m4_test -1 quotes
}

redef_head()
{
}
redef_body()
{
	m4_test redef
}

strangequotes_head()
{
}
strangequotes_body()
{
	m4_test strangequotes
}

translit_head()
{
}
translit_body()
{
	m4_test translit
}

translit2_head()
{
}
translit2_body()
{
	m4_test translit2
}

atf_init_test_cases()
{
	atf_add_test_case args
	atf_add_test_case args2
	atf_add_test_case comments
	atf_add_test_case defn
	atf_add_test_case esyscmd
	atf_add_test_case eval
	atf_add_test_case ff_after_dnl
	atf_add_test_case gnueval
	atf_add_test_case gnuformat
	atf_add_test_case gnupatterns
	atf_add_test_case gnupatterns2
	atf_add_test_case gnuprefix
	atf_add_test_case gnusofterror
	atf_add_test_case gnutranslit2
	atf_add_test_case includes
	atf_add_test_case m4wrap3
	atf_add_test_case patterns
	atf_add_test_case quotes
	atf_add_test_case redef
	atf_add_test_case strangequotes
	atf_add_test_case translit
	atf_add_test_case translit2
}
