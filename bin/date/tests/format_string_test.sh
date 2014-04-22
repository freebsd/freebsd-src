#
# Regression tests for date(1)
#
# Submitted by Edwin Groothuis <edwin@FreeBSD.org>
#
# $FreeBSD$
#

#
# These two date/times have been chosen carefully, they
# create both the single digit and double/multidigit version of
# the values.
#
# To create a new one, make sure you are using the UTC timezone!
#

TEST1=3222243		# 1970-02-07 07:04:03
TEST2=1005600000	# 2001-11-12 21:11:12

check()
{
	local format_string exp_output_1 exp_output_2 output

	format_string=$1
	exp_output_1=$2
	exp_output_2=$3

	# If the second sample text for formatted output has not been
	# passed, assume it should match exactly the first one.
	if [ -z "$exp_output_2" ]; then
		exp_output_2=${exp_output_1}
	fi

	output=$(date -r ${TEST1} +%${format_string})
	atf_check test "${output}" = "$exp_output_1"

	output=$(date -r ${TEST2} +%${format_string})
	atf_check test "${output}" = "$exp_output_2"
}

atf_test_case A_format_string
A_format_string_body()
{
	check A Saturday Monday
}

atf_test_case a_format_string
a_format_string_body()
{
	check a Sat Mon
}

atf_test_case B_format_string
B_format_string_body()
{
	check B February November
}

atf_test_case b_format_string
b_format_string_body()
{
	check b Feb Nov
}

atf_test_case C_format_string
C_format_string_body()
{
	check C 19 20
}

atf_test_case c_format_string
c_format_string_body()
{
	check c "Sat Feb  7 07:04:03 1970" "Mon Nov 12 21:20:00 2001"
}

atf_test_case D_format_string
D_format_string_body()
{
	check D 02/07/70 11/12/01
}

atf_test_case d_format_string
d_format_string_body()
{
	check d 07 12
}

atf_test_case e_format_string
e_format_string_body()
{
	check e " 7" 12
}

atf_test_case F_format_string
F_format_string_body()
{
	check F "1970-02-07" "2001-11-12"
}

atf_test_case G_format_string
G_format_string_body()
{
	check G 1970 2001
}

atf_test_case g_format_string
g_format_string_body()
{
	check g 70 01
}

atf_test_case H_format_string
H_format_string_body()
{
	check H 07 21
}

atf_test_case h_format_string
h_format_string_body()
{
	check h Feb Nov
}

atf_test_case I_format_string
I_format_string_body()
{
	check I 07 09
}

atf_test_case j_format_string
j_format_string_body()
{
	check j 038 316
}

atf_test_case k_format_string
k_format_string_body()
{
	check k " 7" 21
}

atf_test_case l_format_string
l_format_string_body()
{
	check l " 7" " 9"
}

atf_test_case M_format_string
M_format_string_body()
{
	check M 04 20
}

atf_test_case m_format_string
m_format_string_body()
{
	check m 02 11
}

atf_test_case p_format_string
p_format_string_body()
{
	check p AM PM
}

atf_test_case R_format_string
R_format_string_body()
{
	check R 07:04 21:20
}

atf_test_case r_format_string
r_format_string_body()
{
	check r "07:04:03 AM" "09:20:00 PM"
}

atf_test_case S_format_string
S_format_string_body()
{
	check S 03 00
}

atf_test_case s_format_string
s_format_string_body()
{
	check s ${TEST1} ${TEST2}
}

atf_test_case U_format_string
U_format_string_body()
{
	check U 05 45
}

atf_test_case u_format_string
u_format_string_body()
{
	check u 6 1
}

atf_test_case V_format_string
V_format_string_body()
{
	check V 06 46
}

atf_test_case v_format_string
v_format_string_body()
{
	check v " 7-Feb-1970" "12-Nov-2001"
}

atf_test_case W_format_string
W_format_string_body()
{
	check W 05 46
}

atf_test_case w_format_string
w_format_string_body()
{
	check w 6 1
}

atf_test_case X_format_string
X_format_string_body()
{
	check X "07:04:03" "21:20:00"
}

atf_test_case x_format_string
x_format_string_body()
{
	check x "02/07/70" "11/12/01"
}

atf_test_case Y_format_string
Y_format_string_body()
{
	check Y 1970 2001
}

atf_test_case y_format_string
y_format_string_body()
{
	check y 70 01
}

atf_test_case Z_format_string
Z_format_string_body()
{
	check Z UTC UTC
}

atf_test_case z_format_string
z_format_string_body()
{
	check z +0000 +0000
}

atf_test_case percent_format_string
percent_format_string_body()
{
	check % % %
}

atf_test_case plus_format_string
plus_format_string_body()
{
	check + "Sat Feb  7 07:04:03 UTC 1970" "Mon Nov 12 21:20:00 UTC 2001"
}

atf_init_test_cases()
{
	atf_add_test_case A_format_string
	atf_add_test_case a_format_string
	atf_add_test_case B_format_string
	atf_add_test_case b_format_string
	atf_add_test_case C_format_string
	atf_add_test_case c_format_string
	atf_add_test_case D_format_string
	atf_add_test_case d_format_string
	atf_add_test_case e_format_string
	atf_add_test_case F_format_string
	atf_add_test_case G_format_string
	atf_add_test_case g_format_string
	atf_add_test_case H_format_string
	atf_add_test_case h_format_string
	atf_add_test_case I_format_string
	atf_add_test_case j_format_string
	atf_add_test_case k_format_string
	atf_add_test_case l_format_string
	atf_add_test_case M_format_string
	atf_add_test_case m_format_string
	atf_add_test_case p_format_string
	atf_add_test_case R_format_string
	atf_add_test_case r_format_string
	atf_add_test_case S_format_string
	atf_add_test_case s_format_string
	atf_add_test_case U_format_string
	atf_add_test_case u_format_string
	atf_add_test_case V_format_string
	atf_add_test_case v_format_string
	atf_add_test_case W_format_string
	atf_add_test_case w_format_string
	atf_add_test_case X_format_string
	atf_add_test_case x_format_string
	atf_add_test_case Y_format_string
	atf_add_test_case y_format_string
	atf_add_test_case Z_format_string
	atf_add_test_case z_format_string
	atf_add_test_case percent_format_string
	atf_add_test_case plus_format_string
}
