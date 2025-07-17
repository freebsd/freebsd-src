#
# Copyright 2017 Shivansh
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
#

atf_test_case c_flag
c_flag_head()
{
	atf_set "descr" "Verify the usage of option 'c'"
}

c_flag_body()
{
	atf_check -s exit:0 -o inline:"1  2  3  4
5  6  7  8
" rs -c, 2 <<EOF
1,2,3,4,5,6,7,8
EOF
}

atf_test_case s_flag
s_flag_head()
{
	atf_set "descr" "Verify the usage of option 's'"
}

s_flag_body()
{
	atf_check -s exit:0 -o inline:"1  2  3  4
5  6  7  8
" rs -s% 2 <<EOF
1%%2%%3%4%5%%%6%%%7%8
EOF

}

atf_test_case C_flag
C_flag_head()
{
	atf_set "descr" "Verify the usage of option 'C'"
}

C_flag_body()
{
	atf_check -s exit:0 -o inline:"1,2,3,4,
5,6,7,8,
" rs -C, 2 <<EOF
1 2 3 4 5 6 7 8
EOF
}

atf_test_case S_flag
S_flag_head()
{
	atf_set "descr" "Verify the usage of option 'S'"
}

S_flag_body()
{
	atf_check -s exit:0 -o inline:"1,,2,,3,,4
5,,6,,7,,8
" rs -S, 2 <<EOF
1 2 3 4 5 6 7 8
EOF
}

atf_test_case t_flag
t_flag_head()
{
	atf_set "descr" "Verify the usage of option 't'"
}

t_flag_body()
{
	atf_check -s exit:0 -o inline:"1  4  7
2  5  8
3  6  
" rs -t 3 <<EOF
1 2 3 4 5 6 7 8
EOF
}

atf_test_case T_flag
T_flag_head()
{
	atf_set "descr" "Verify the usage of option 'T'"
}

T_flag_body()
{
	atf_check -s exit:0 -o inline:"1
2
3
" rs -T <<EOF
1 2 3
EOF
}

atf_test_case k_flag
k_flag_head()
{
	atf_set "descr" "Verify the usage of option 'k'"
}

k_flag_body()
{
	atf_check -s exit:0 -o inline:"3  4  5
6  7  8
" rs -k 1 2 <<EOF
1 2
3 4 5 6
7 8
EOF
}

atf_test_case K_flag
K_flag_head()
{
	atf_set "descr" "Verify the usage of option 'K'"
}

K_flag_body()
{
	atf_check -s exit:0 -o inline:"
" rs -K 1 < /dev/null
}

atf_test_case g_flag
g_flag_head()
{
	atf_set "descr" "Verify the usage of option 'g'"
}

g_flag_body()
{
	atf_check -s exit:0 -o inline:"1 2 3 4
5 6 7 8
" rs -g 1 2 <<EOF
1 2 3 4 5 6 7 8
EOF
}

atf_test_case G_flag
G_flag_head()
{
	atf_set "descr" "Verify the usage of option 'G'"
}

G_flag_body()
{
	atf_check -s exit:0 -o inline:"1                2                3                4
5                6                7                acbdefghij
" rs -G 50 2 <<EOF
1 2 3 4 5 6 7 acbdefghij
EOF
}

atf_test_case e_flag
e_flag_head()
{
	atf_set "descr" "Verify the usage of option 'e'"
}

e_flag_body()
{
	atf_check -s exit:0 -o inline:"
" rs -e < /dev/null
}

atf_test_case n_flag
n_flag_head()
{
	atf_set "descr" "Verify the usage of option 'n'"
}

n_flag_body()
{
	atf_check -s exit:0 -o inline:"1  2  3
4  5  
6  7  
" rs -n 0 3 <<EOF
1 2 3
4 5
6 7
EOF
}

atf_test_case y_flag
y_flag_head()
{
	atf_set "descr" "Verify the usage of option 'y'"
}

y_flag_body()
{
	atf_check -s exit:0 -o inline:"1  2  3
4  5  6
7  8  1
" rs -y 3 <<EOF
1 2 3 4 5 6 7 8
EOF
}

atf_test_case h_flag
h_flag_head()
{
	atf_set "descr" "Verify the usage of option 'h'"
}

h_flag_body()
{
	atf_check -s exit:0 -o inline:"1 0
" rs -h < /dev/null
}

atf_test_case H_flag
H_flag_head()
{
	atf_set "descr" "Verify the usage of option 'H'"
}

H_flag_body()
{
	atf_check -s exit:0 -o inline:" 0 line 1
1 0
" rs -H < /dev/null
}

atf_test_case j_flag
j_flag_head()
{
	atf_set "descr" "Verify the usage of option 'j'"
}

j_flag_body()
{
	atf_check -s exit:0 -o inline:"  1    2    3
abc  def  ghi
" rs -j 2 <<EOF
1 2 3 abc def ghi
EOF
}

atf_test_case m_flag
m_flag_head()
{
	atf_set "descr" "Verify the usage of option 'm'"
}

m_flag_body()
{
	atf_check -s exit:0 -o inline:"1    2    3    
abc  def  ghi  
" rs -m 2 <<EOF
1 2 3 abc def ghi
EOF
}

atf_test_case z_flag
z_flag_head()
{
	atf_set "descr" "Verify the usage of option 'z'"
}

z_flag_body()
{
	atf_check -s exit:0 -o inline:"1  2  3  4
5  6  7  acbdefghij
" rs -z 2 <<EOF
1 2 3 4 5 6 7 acbdefghij
EOF
}

atf_test_case invalid_usage
invalid_usage_head()
{
	atf_set "descr" "Verify that an invalid usage with a supported option produces a valid error message"
}

invalid_usage_body()
{
	atf_check -s not-exit:0 -e inline:"rs: option requires an argument -- w
usage: rs [-[csCS][x][kKgGw][N]tTeEnyjhHmz] [rows [cols]]
" rs -w
}

atf_test_case no_arguments
no_arguments_head()
{
	atf_set "descr" "Verify that rs(1) executes successfully and produces a valid output when invoked without any arguments"
}

no_arguments_body()
{
	atf_check -s exit:0 -o inline:"1 2 3 4 5 6 7 8
" rs <<EOF
1 2 3 4 5 6 7 8
EOF
}

atf_test_case rows_2
rows_2_head()
{
	atf_set "descr" "Simple output with 2 rows"
}

rows_2_body()
{
	atf_check -s exit:0 -o inline:"1  2  3  4
5  6  7  8
" rs 2 <<EOF
1 2 3 4 5 6 7 8
EOF
}

atf_test_case rows_3
rows_3_head()
{
	atf_set "descr" "Simple output with 3 rows"
}

rows_3_body()
{
	atf_check -s exit:0 -o inline:"1  2  3
4  5  6
7  8  
" rs 3 <<EOF
1 2 3 4 5 6 7 8
EOF
}

atf_test_case rows_4
rows_4_head()
{
	atf_set "descr" "Simple output with 4 rows"
}

rows_4_body()
{
	atf_check -s exit:0 -o inline:"1  2
3  4
5  6
7  8
" rs 4 <<EOF
1 2 3 4 5 6 7 8
EOF
}

atf_init_test_cases()
{
	atf_add_test_case c_flag
	atf_add_test_case s_flag
	atf_add_test_case C_flag
	atf_add_test_case S_flag
	atf_add_test_case t_flag
	atf_add_test_case T_flag
	atf_add_test_case k_flag
	atf_add_test_case K_flag
	atf_add_test_case g_flag
	atf_add_test_case G_flag
	atf_add_test_case e_flag
	atf_add_test_case n_flag
	atf_add_test_case y_flag
	atf_add_test_case h_flag
	atf_add_test_case H_flag
	atf_add_test_case j_flag
	atf_add_test_case m_flag
	atf_add_test_case z_flag
	atf_add_test_case invalid_usage
	atf_add_test_case no_arguments
	atf_add_test_case rows_2
	atf_add_test_case rows_3
	atf_add_test_case rows_4
}
