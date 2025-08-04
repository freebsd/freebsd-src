#
# Copyright (c) 2002 Juli Mallett <jmallett@FreeBSD.org>
# Copyright (c) 2025 Dag-Erling Smørgrav <des@FreeBSD.org>
#
# SPDX-License-Identifier: BSD-2-Clause
#

SRCDIR=$(atf_get_srcdir)

atf_test_case xargs_normal
xargs_normal_body()
{
	atf_check -o file:${SRCDIR}/regress.normal.out \
	    xargs echo The <${SRCDIR}/regress.in
}

atf_test_case xargs_I
xargs_I_body()
{
	atf_check -o file:${SRCDIR}/regress.I.out \
	    xargs -I% echo The % % % %% % % <${SRCDIR}/regress.in
}

atf_test_case xargs_J
xargs_J_body()
{
	atf_check -o file:${SRCDIR}/regress.J.out \
	    xargs -J% echo The % again. <${SRCDIR}/regress.in
}

atf_test_case xargs_L
xargs_L_body()
{
	atf_check -o file:${SRCDIR}/regress.L.out \
	    xargs -L3 echo <${SRCDIR}/regress.in
}

atf_test_case xargs_P1
xargs_P1_body()
{
	atf_check -o file:${SRCDIR}/regress.P1.out \
	    xargs -P1 echo <${SRCDIR}/regress.in
}

atf_test_case xargs_R
xargs_R_body()
{
	atf_check -o file:${SRCDIR}/regress.R.out \
	    xargs -I% -R1 echo The % % % %% % % <${SRCDIR}/regress.in
}

atf_test_case xargs_R_1
xargs_R_1_body()
{
	atf_check -o file:${SRCDIR}/regress.R-1.out \
	    xargs -I% -R-1 echo The % % % %% % % <${SRCDIR}/regress.in
}

atf_test_case xargs_n1
xargs_n1_body()
{
	atf_check -o file:${SRCDIR}/regress.n1.out \
	    xargs -n1 echo <${SRCDIR}/regress.in
}

atf_test_case xargs_n2
xargs_n2_body()
{
	atf_check -o file:${SRCDIR}/regress.n2.out \
	    xargs -n2 echo <${SRCDIR}/regress.in
}

atf_test_case xargs_nargmax
xargs_nargmax_body()
{
	argmax=$(sysctl -n kern.argmax)
	atf_check -o file:${SRCDIR}/regress.nargmax.out \
	    xargs -n$((argmax)) <${SRCDIR}/regress.in
	atf_check -s exit:1 -e match:"too large" \
	    xargs -n$((argmax+1)) <${SRCDIR}/regress.in
}

atf_test_case xargs_n2P0
xargs_n2P0_body()
{
	atf_check -o save:regress.out \
	    xargs -n2 -P0 echo <${SRCDIR}/regress.in
	atf_check -o file:${SRCDIR}/regress.n2P0.out \
	    sort regress.out
}

atf_test_case xargs_n3
xargs_n3_body()
{
	atf_check -o file:${SRCDIR}/regress.n3.out \
	    xargs -n3 echo <${SRCDIR}/regress.in
}

atf_test_case xargs_0
xargs_0_body()
{
	atf_check -o file:${SRCDIR}/regress.0.out \
	    xargs -0 -n1 echo <${SRCDIR}/regress.0.in
}

atf_test_case xargs_0I
xargs_0I_body()
{
	atf_check -o file:${SRCDIR}/regress.0I.out \
	    xargs -0 -I% echo The % %% % <${SRCDIR}/regress.0.in
}

atf_test_case xargs_0J
xargs_0J_body()
{
	atf_check -o file:${SRCDIR}/regress.0J.out \
	    xargs -0 -J% echo The % again. <${SRCDIR}/regress.0.in
}

atf_test_case xargs_0L
xargs_0L_body()
{
	atf_check -o file:${SRCDIR}/regress.0L.out \
	    xargs -0 -L2 echo <${SRCDIR}/regress.0.in
}

atf_test_case xargs_0P1
xargs_0P1_body()
{
	atf_check -o file:${SRCDIR}/regress.0P1.out \
	    xargs -0 -P1 echo <${SRCDIR}/regress.0.in
}

atf_test_case xargs_quotes
xargs_quotes_body()
{
	atf_check -o file:${SRCDIR}/regress.quotes.out \
	    xargs -n1 echo <${SRCDIR}/regress.quotes.in
}

atf_test_case xargs_parallel1
xargs_parallel1_body()
{
	echo /var/empty /var/empty >input
	atf_check xargs -n1 -P2 test -d <input
}

atf_test_case xargs_parallel2
xargs_parallel2_body()
{
	echo /var/empty /var/empty/nodir >input
	atf_check -s exit:1 xargs -n1 -P2 test -d <input
}

atf_test_case xargs_parallel3
xargs_parallel3_body()
{
	echo /var/empty/nodir /var/empty >input
	atf_check -s exit:1 xargs -n1 -P2 test -d <input
}

atf_test_case xargs_parallel4
xargs_parallel4_body()
{
	echo /var/empty/nodir /var/empty/nodir >input
	atf_check -s exit:1 xargs -n1 -P2 test -d <input
}

atf_init_test_cases()
{
	atf_add_test_case xargs_normal
	atf_add_test_case xargs_I
	atf_add_test_case xargs_J
	atf_add_test_case xargs_L
	atf_add_test_case xargs_P1
	atf_add_test_case xargs_R
	atf_add_test_case xargs_R_1
	atf_add_test_case xargs_n1
	atf_add_test_case xargs_n2
	atf_add_test_case xargs_nargmax
	atf_add_test_case xargs_n2P0
	atf_add_test_case xargs_n3
	atf_add_test_case xargs_0
	atf_add_test_case xargs_0I
	atf_add_test_case xargs_0J
	atf_add_test_case xargs_0L
	atf_add_test_case xargs_0P1
	atf_add_test_case xargs_quotes
	atf_add_test_case xargs_parallel1
	atf_add_test_case xargs_parallel2
	atf_add_test_case xargs_parallel3
	atf_add_test_case xargs_parallel4
}
