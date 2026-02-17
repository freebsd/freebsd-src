# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2025 Baptiste Daroussin <bapt@FreeBSD.org>

# Helper: create standard 5-line data file
create_std_data()
{
	cat > "$1" <<'EOF'
line 1
line 2
line 3
line 4
line5
EOF
}

# ---------------------------------------------------------------------------
# Append (a)
# ---------------------------------------------------------------------------
atf_test_case append
append_head()
{
	atf_set "descr" "Test append command (a)"
}

append_body()
{
	create_std_data input.txt
	ed -s - <<'CMDS'
H
r input.txt
0a
hello world
.
2a
hello world!
.
$a
hello world!!
.
w output.txt
CMDS
	cat > expected.txt <<'EOF'
hello world
line 1
hello world!
line 2
line 3
line 4
line5
hello world!!
EOF
	atf_check cmp output.txt expected.txt
}

# ---------------------------------------------------------------------------
# Address parsing (addr)
# ---------------------------------------------------------------------------
atf_test_case address
address_head()
{
	atf_set "descr" "Test complex address parsing"
}
address_body()
{
	cat > input.txt <<'EOF'
line 1
line 2
line 3
line 4
line5
1ine6
line7
line8
line9
EOF
	ed -s - <<'CMDS'
H
r input.txt
1 d
1 1 d
1,2,d
1;+ + ,d
1,2;., + 2d
w output.txt
CMDS
	cat > expected.txt <<'EOF'
line 2
line9
EOF
	atf_check cmp output.txt expected.txt
}

# ---------------------------------------------------------------------------
# Change (c)
# ---------------------------------------------------------------------------
atf_test_case change
change_head()
{
	atf_set "descr" "Test change command (c)"
}
change_body()
{
	create_std_data input.txt
	ed -s - <<'CMDS'
H
r input.txt
1c
at the top
.
4c
in the middle
.
$c
at the bottom
.
2,3c
between top/middle
.
w output.txt
CMDS
	cat > expected.txt <<'EOF'
at the top
between top/middle
in the middle
at the bottom
EOF
	atf_check cmp output.txt expected.txt
}

# ---------------------------------------------------------------------------
# Delete (d)
# ---------------------------------------------------------------------------
atf_test_case delete
delete_head()
{
	atf_set "descr" "Test delete command (d)"
}
delete_body()
{
	create_std_data input.txt
	ed -s - <<'CMDS'
H
r input.txt
1d
2;+1d
$d
w output.txt
CMDS
	printf 'line 2\n' > expected.txt
	atf_check cmp output.txt expected.txt
}

# ---------------------------------------------------------------------------
# Insert (i)
# ---------------------------------------------------------------------------
atf_test_case insert
insert_head()
{
	atf_set "descr" "Test insert command (i)"
}
insert_body()
{
	create_std_data input.txt
	ed -s - <<'CMDS'
H
r input.txt
1i
hello world
.
2i
hello world!
.
$i
hello world!!
.
w output.txt
CMDS
	cat > expected.txt <<'EOF'
hello world
hello world!
line 1
line 2
line 3
line 4
hello world!!
line5
EOF
	atf_check cmp output.txt expected.txt
}

# ---------------------------------------------------------------------------
# Join (j)
# ---------------------------------------------------------------------------
atf_test_case join
join_head()
{
	atf_set "descr" "Test join command (j)"
}
join_body()
{
	create_std_data input.txt
	ed -s - <<'CMDS'
H
r input.txt
1,1j
2,3j
w output.txt
CMDS
	cat > expected.txt <<'EOF'
line 1
line 2line 3
line 4
line5
EOF
	atf_check cmp output.txt expected.txt
}

# ---------------------------------------------------------------------------
# Mark (k)
# ---------------------------------------------------------------------------
atf_test_case mark
mark_head()
{
	atf_set "descr" "Test mark and reference commands (k, ')"
}
mark_body()
{
	create_std_data input.txt
	ed -s - <<'CMDS'
H
r input.txt
2ka
1d
'am$
1ka
0a
hello world
.
'ad
u
'am0
w output.txt
CMDS
	cat > expected.txt <<'EOF'
line 3
hello world
line 4
line5
line 2
EOF
	atf_check cmp output.txt expected.txt
}

# ---------------------------------------------------------------------------
# Move (m)
# ---------------------------------------------------------------------------
atf_test_case move
move_head()
{
	atf_set "descr" "Test move command (m)";
}
move_body()
{

	create_std_data input.txt
	ed -s - <<'CMDS'
H
r input.txt
1,2m$
1,2m$
1,2m$
$m0
$m0
2,3m1
2,3m3
w output.txt
CMDS
	cat > expected.txt <<'EOF'
line5
line 1
line 2
line 3
line 4
EOF
	atf_check cmp output.txt expected.txt
}

# ---------------------------------------------------------------------------
# Transfer / Copy (t)
# ---------------------------------------------------------------------------
atf_test_case transfer
transfer_head()
{
	atf_set "descr" "Test transfer/copy command (t)";
}
transfer_body()
{

	create_std_data input.txt
	ed -s - <<'CMDS'
H
r input.txt
1t0
2,3t2
,t$
w output.txt
CMDS
	cat > expected.txt <<'EOF'
line 1
line 1
line 1
line 2
line 2
line 3
line 4
line5
line 1
line 1
line 1
line 2
line 2
line 3
line 4
line5
EOF
	atf_check cmp output.txt expected.txt
}

atf_test_case transfer_search
transfer_search_head()
{
	atf_set "descr" "Test transfer with address search (t)";
}
transfer_search_body()
{

	create_std_data input.txt
	ed -s - <<'CMDS'
H
r input.txt
t0;/./
w output.txt
CMDS
	cat > expected.txt <<'EOF'
line 1
line5
line 2
line 3
line 4
line5
EOF
	atf_check cmp output.txt expected.txt
}

# ---------------------------------------------------------------------------
# Undo (u)
# ---------------------------------------------------------------------------
atf_test_case undo
undo_head()
{
	atf_set "descr" "Test undo command (u)";
}
undo_body()
{

	create_std_data input.txt
	printf 'dummy\n' > readfile.txt
	ed -s - <<'CMDS'
H
r input.txt
1;r readfile.txt
u
a
hello
world
.
g/./s//x/\
a\
hello\
world
u
u
u
a
hello world!
.
u
1,$d
u
2,3d
u
c
hello world!!
.
u
u
-1;.,+1j
u
u
u
.,+1t$
w output.txt
CMDS
	cat > expected.txt <<'EOF'
line 1
hello
hello world!!
line 2
line 3
line 4
line5
hello
hello world!!
EOF
	atf_check cmp output.txt expected.txt
}

# ---------------------------------------------------------------------------
# Global (g)
# ---------------------------------------------------------------------------
atf_test_case global_move
global_move_head()
{
	atf_set "descr" "Test global command with move (g)";
}
global_move_body()
{

	create_std_data input.txt
	ed -s - <<'CMDS'
H
r input.txt
g/./m0
g/./s/$/\
hello world
g/hello /s/lo/p!/\
a\
order
w output.txt
CMDS
	cat > expected.txt <<'EOF'
line5
help! world
order
line 4
help! world
order
line 3
help! world
order
line 2
help! world
order
line 1
help! world
order
EOF
	atf_check cmp output.txt expected.txt
}

atf_test_case global_change
global_change_head()
{
	atf_set "descr" "Test global command with change (g)";
}
global_change_body()
{

	create_std_data input.txt
	ed -s - <<'CMDS'
H
r input.txt
g/[2-4]/-1,+1c\
hello world
w output.txt
CMDS
	printf 'hello world\n' > expected.txt
	atf_check cmp output.txt expected.txt
}

atf_test_case global_substitute
global_substitute_head()
{
	atf_set "descr" "Test global with substitute and move (g)";
}
global_substitute_body()
{

	create_std_data input.txt
	ed -s - <<'CMDS'
H
r input.txt
g/./s//x/\
3m0
g/./s/e/c/\
2,3m1
w output.txt
CMDS
	cat > expected.txt <<'EOF'
linc 3
xine 1
xine 2
xinc 4
xinc5
EOF
	atf_check cmp output.txt expected.txt
}

atf_test_case global_undo
global_undo_head()
{
	atf_set "descr" "Test global with undo (g)";
}
global_undo_body()
{

	create_std_data input.txt
	ed -s - <<'CMDS'
H
r input.txt
g/./s/./x/\
u\
s/./y/\
u\
s/./z/\
u
u
0a
hello
.
$a
world
.
w output.txt
CMDS
	cat > expected.txt <<'EOF'
hello
zine 1
line 2
line 3
line 4
line5
world
EOF
	atf_check cmp output.txt expected.txt
}

atf_test_case global_copy
global_copy_head()
{
	atf_set "descr" "Test global with copy (g)";
}
global_copy_body()
{

	cat > input.txt <<'EOF'
line 1
line 2
line 3
EOF
	ed -s - <<'CMDS'
H
r input.txt
g/./1,3t$\
1d
w output.txt
CMDS
	cat > expected.txt <<'EOF'
line 1
line 2
line 3
line 2
line 3
line 1
line 3
line 1
line 2
EOF
	atf_check cmp output.txt expected.txt
}

# ---------------------------------------------------------------------------
# Inverse global (v)
# ---------------------------------------------------------------------------
atf_test_case inverse_global
inverse_global_head()
{
	atf_set "descr" "Test inverse global command (v)";
}
inverse_global_body()
{

	create_std_data input.txt
	ed -s - <<'CMDS'
H
r input.txt
v/[ ]/m0
v/[ ]/s/$/\
hello world
v/hello /s/lo/p!/\
a\
order
w output.txt
CMDS
	cat > expected.txt <<'EOF'
line5
order
hello world
line 1
order
line 2
order
line 3
order
line 4
order
EOF
	atf_check cmp output.txt expected.txt
}

# ---------------------------------------------------------------------------
# Substitution (s)
# ---------------------------------------------------------------------------
atf_test_case subst_backreference
subst_backreference_head()
{
	atf_set "descr" "Test substitute with backreferences (s)";
}
subst_backreference_body()
{

	create_std_data input.txt
	ed -s - <<'CMDS'
H
r input.txt
s/\([^ ][^ ]*\)/(\1)/g
2s
/3/s
/\(4\)/sr
/\(.\)/srg
%s/i/&e/
w output.txt
CMDS
	cat > expected.txt <<'EOF'
liene 1
(liene) (2)
(liene) (3)
liene (4)
(()liene5)
EOF
	atf_check cmp output.txt expected.txt
}

atf_test_case subst_range
subst_range_head()
{
	atf_set "descr" "Test substitute on range with count and repeat (s)";
}
subst_range_body()
{

	create_std_data input.txt
	ed -s - <<'CMDS'
H
r input.txt
,s/./(&)/3
s/$/00
2s//%/g
s/^l
w output.txt
CMDS
	cat > expected.txt <<'EOF'
li(n)e 1
i(n)e 200
li(n)e 3
li(n)e 4
li(n)e500
EOF
	atf_check cmp output.txt expected.txt
}

atf_test_case subst_charclass
subst_charclass_head()
{
	atf_set "descr" "Test substitute with character classes (s)";
}
subst_charclass_body()
{

	ed -s - <<'CMDS'
H
a
hello/[]world
.
s/[/]/ /
s/[[:digit:][]/ /
s/[]]/ /
w output.txt
CMDS
	printf 'hello   world\n' > expected.txt
	atf_check cmp output.txt expected.txt
}

# ---------------------------------------------------------------------------
# Edit (e/E)
# ---------------------------------------------------------------------------
atf_test_case edit_file
edit_file_head()
{
	atf_set "descr" "Test edit file command (E)";
}
edit_file_body()
{

	printf 'hello world\n' > input.txt
	printf 'E e1_data.txt\n' > e1_data.txt
	ed -s - <<'CMDS'
H
r input.txt
E e1_data.txt
w output.txt
CMDS
	printf 'E e1_data.txt\n' > expected.txt
	atf_check cmp output.txt expected.txt
}

atf_test_case edit_command
edit_command_head()
{
	atf_set "descr" "Test edit with shell command (E !)";
}
edit_command_body()
{

	printf 'E !echo hello world-\n' > input.txt
	ed -s - <<'CMDS'
H
r input.txt
E !echo hello world-
w output.txt
CMDS
	printf 'hello world-\n' > expected.txt
	atf_check cmp output.txt expected.txt
}

atf_test_case edit_reread
edit_reread_head()
{
	atf_set "descr" "Test edit re-read default file (E)";
}
edit_reread_body()
{

	printf 'E !echo hello world-\n' > input.txt
	ed -s - <<'CMDS'
H
r input.txt
E
w output.txt
CMDS
	printf 'E !echo hello world-\n' > expected.txt
	atf_check cmp output.txt expected.txt
}

atf_test_case edit_lowercase
edit_lowercase_head()
{
	atf_set "descr" "Test lowercase edit re-read (e)";
}
edit_lowercase_body()
{

	printf 'E !echo hello world-\n' > input.txt
	ed -s - <<'CMDS'
H
r input.txt
e
w output.txt
CMDS
	printf 'E !echo hello world-\n' > expected.txt
	atf_check cmp output.txt expected.txt
}

# ---------------------------------------------------------------------------
# Read (r)
# ---------------------------------------------------------------------------
atf_test_case read_command
read_command_head()
{
	atf_set "descr" "Test read with shell command (r !)";
}
read_command_body()
{

	create_std_data input.txt
	ed -s - <<'CMDS'
H
r input.txt
1;r !echo hello world
1
r !echo hello world
w output.txt
CMDS
	cat > expected.txt <<'EOF'
line 1
hello world
line 2
line 3
line 4
line5
hello world
EOF
	atf_check cmp output.txt expected.txt
}

atf_test_case read_default
read_default_head()
{
	atf_set "descr" "Test read with default filename (r)";
}
read_default_body()
{

	create_std_data input.txt
	ed -s - <<'CMDS'
H
r input.txt
r
w output.txt
CMDS
	cat > expected.txt <<'EOF'
line 1
line 2
line 3
line 4
line5
line 1
line 2
line 3
line 4
line5
EOF
	atf_check cmp output.txt expected.txt
}

atf_test_case read_file
read_file_head()
{
	atf_set "descr" "Test read from file (r)";
}
read_file_body()
{

	printf 'r r3_data.txt\n' > r3_data.txt
	ed -s - <<'CMDS'
H
r r3_data.txt
r r3_data.txt
w output.txt
CMDS
	cat > expected.txt <<'EOF'
r r3_data.txt
r r3_data.txt
EOF
	atf_check cmp output.txt expected.txt
}

# ---------------------------------------------------------------------------
# Write (w)
# ---------------------------------------------------------------------------
atf_test_case write_pipe
write_pipe_head()
{
	atf_set "descr" "Test write to shell command (w !)";
}
write_pipe_body()
{

	create_std_data input.txt
	ed -s - <<'CMDS'
H
r input.txt
w !cat >\!.z
r \!.z
w output.txt
CMDS
	cat > expected.txt <<'EOF'
line 1
line 2
line 3
line 4
line5
line 1
line 2
line 3
line 4
line5
EOF
	atf_check cmp output.txt expected.txt
}

# ---------------------------------------------------------------------------
# Quit (q)
# ---------------------------------------------------------------------------
atf_test_case quit
quit_head()
{
	atf_set "descr" "Test quit command (q)";
}
quit_body()
{

	ed -s - <<'CMDS'
H
w output.txt
a
hello
.
q
CMDS
	atf_check -s exit:0 test ! -s output.txt
}

# ---------------------------------------------------------------------------
# Shell command (!)
# ---------------------------------------------------------------------------
atf_test_case shell_command
shell_command_head()
{
	atf_set "descr" "Test shell command execution (!)";
}
shell_command_body()
{

	ed -s - <<'CMDS'
H
!read one
hello, world
a
okay
.
w output.txt
CMDS
	printf 'okay\n' > expected.txt
	atf_check cmp output.txt expected.txt
}

# ---------------------------------------------------------------------------
# Newline handling (nl)
# ---------------------------------------------------------------------------
atf_test_case newline_insert
newline_insert_head()
{
	atf_set "descr" "Test inserting blank lines";
}
newline_insert_body()
{

	create_std_data input.txt
	ed -s - <<'CMDS'
H
r input.txt
1


0a


hello world
.
w output.txt
CMDS
	cat > expected.txt <<'EOF'


hello world
line 1
line 2
line 3
line 4
line5
EOF
	atf_check cmp output.txt expected.txt
}

atf_test_case newline_search
newline_search_head()
{
	atf_set "descr" "Test address search with semicolon";
}
newline_search_body()
{

	create_std_data input.txt
	ed -s - <<'CMDS'
H
r input.txt
a
hello world
.
0;/./
w output.txt
CMDS
	cat > expected.txt <<'EOF'
line 1
line 2
line 3
line 4
line5
hello world
EOF
	atf_check cmp output.txt expected.txt
}

# ---------------------------------------------------------------------------
# Error tests
# ---------------------------------------------------------------------------
atf_test_case err_append_suffix
err_append_suffix_head()
{
	atf_set "descr" "Error: invalid append suffix (aa)";
}
err_append_suffix_body()
{

	atf_check -s exit:2 -e not-empty ed -s - <<'CMDS'
H
aa
hello world
.
CMDS
}

atf_test_case err_addr_out_of_range
err_addr_out_of_range_head()
{
	atf_set "descr" "Error: address out of range";
}
err_addr_out_of_range_body()
{

	atf_check -s exit:2 -e not-empty ed -s - <<'CMDS'
H
100
CMDS
}

atf_test_case err_addr_negative
err_addr_negative_head()
{
	atf_set "descr" "Error: negative address";
}
err_addr_negative_body()
{

	atf_check -s exit:2 -e not-empty ed -s - <<'CMDS'
H
-100
CMDS
}

atf_test_case err_bang_addr
err_bang_addr_head()
{
	atf_set "descr" "Error: shell command with address";
}
err_bang_addr_body()
{

	printf 'test\n' > input.txt
	atf_check -s exit:2 -e not-empty ed -s - <<'CMDS'
H
r input.txt
.!date
CMDS
}

atf_test_case err_bang_double
err_bang_double_head()
{
	atf_set "descr" "Error: double bang without previous command";
}
err_bang_double_body()
{

	atf_check -s exit:2 -e not-empty ed -s - <<'CMDS'
H
!!
CMDS
}

atf_test_case err_change_suffix
err_change_suffix_head()
{
	atf_set "descr" "Error: invalid change suffix (cc)";
}
err_change_suffix_body()
{

	atf_check -s exit:2 -e not-empty ed -s - <<'CMDS'
H
cc
hello world
.
CMDS
}

atf_test_case err_change_zero
err_change_zero_head()
{
	atf_set "descr" "Error: change at line 0";
}
err_change_zero_body()
{

	atf_check -s exit:2 -e not-empty ed -s - <<'CMDS'
H
0c
hello world
.
CMDS
}

atf_test_case err_delete_suffix
err_delete_suffix_head()
{
	atf_set "descr" "Error: invalid delete suffix (dd)";
}
err_delete_suffix_body()
{

	atf_check -s exit:2 -e not-empty ed -s - <<'CMDS'
H
dd
CMDS
}

atf_test_case err_edit_suffix
err_edit_suffix_head()
{
	atf_set "descr" "Error: invalid edit suffix (ee)";
}
err_edit_suffix_body()
{

	printf 'test\n' > e1.err
	atf_check -s exit:2 -e not-empty ed -s - <<'CMDS'
H
ee e1.err
CMDS
}

atf_test_case err_edit_addr
err_edit_addr_head()
{
	atf_set "descr" "Error: edit with address";
}
err_edit_addr_body()
{

	printf 'test\n' > e2.err
	atf_check -s exit:2 -e not-empty ed -s - <<'CMDS'
H
r e2.err
.e e2.err
CMDS
}

atf_test_case err_edit_nosuffix
err_edit_nosuffix_head()
{
	atf_set "descr" "Error: edit without space before filename";
}
err_edit_nosuffix_body()
{

	printf 'test\n' > ee.err
	atf_check -s exit:2 -e not-empty ed -s - <<'CMDS'
H
ee.err
CMDS
}

atf_test_case err_file_addr
err_file_addr_head()
{
	atf_set "descr" "Error: file command with address";
}
err_file_addr_body()
{

	printf 'test\n' > input.txt
	atf_check -s exit:2 -e not-empty ed -s - <<'CMDS'
H
r input.txt
.f f1.err
CMDS
}

atf_test_case err_file_suffix
err_file_suffix_head()
{
	atf_set "descr" "Error: invalid file suffix";
}
err_file_suffix_body()
{

	atf_check -s exit:2 -e not-empty ed -s - <<'CMDS'
H
ff1.err
CMDS
}

atf_test_case err_global_delim
err_global_delim_head()
{
	atf_set "descr" "Error: invalid pattern delimiter in global";
}
err_global_delim_body()
{

	printf 'test\n' > input.txt
	atf_check -s exit:2 -e not-empty ed -s - <<'CMDS'
H
r input.txt
g/./s //x/
CMDS
}

atf_test_case err_global_empty
err_global_empty_head()
{
	atf_set "descr" "Error: empty pattern in global";
}
err_global_empty_body()
{

	printf 'test\n' > input.txt
	atf_check -s exit:2 -e not-empty ed -s - <<'CMDS'
H
r input.txt
g//s/./x/
CMDS
}

atf_test_case err_global_incomplete
err_global_incomplete_head()
{
	atf_set "descr" "Error: incomplete global command";
}
err_global_incomplete_body()
{

	atf_check -s exit:2 -e not-empty ed -s - <<'CMDS'
H
g
CMDS
}

atf_test_case err_help_addr
err_help_addr_head()
{
	atf_set "descr" "Error: help with address";
}
err_help_addr_body()
{

	printf 'test\n' > input.txt
	atf_check -s exit:2 -e not-empty ed -s - <<'CMDS'
H
r input.txt
.h
CMDS
}

atf_test_case err_insert_suffix
err_insert_suffix_head()
{
	atf_set "descr" "Error: invalid insert suffix (ii)";
}
err_insert_suffix_body()
{

	atf_check -s exit:2 -e not-empty ed -s - <<'CMDS'
H
ii
hello world
.
CMDS
}

atf_test_case err_insert_zero
err_insert_zero_head()
{
	atf_set "descr" "Error: insert at line 0";
}
err_insert_zero_body()
{

	atf_check -s exit:2 -e not-empty ed -s - <<'CMDS'
H
0i
hello world
.
CMDS
}

atf_test_case err_mark_upper
err_mark_upper_head()
{
	atf_set "descr" "Error: mark with uppercase letter";
}
err_mark_upper_body()
{

	printf 'test\n' > input.txt
	atf_check -s exit:2 -e not-empty ed -s - <<'CMDS'
H
r input.txt
kA
CMDS
}

atf_test_case err_mark_zero
err_mark_zero_head()
{
	atf_set "descr" "Error: mark at line 0";
}
err_mark_zero_body()
{

	atf_check -s exit:2 -e not-empty ed -s - <<'CMDS'
H
0ka
CMDS
}

atf_test_case err_mark_ref
err_mark_ref_head()
{
	atf_set "descr" "Error: reference to deleted mark";
}
err_mark_ref_body()
{

	atf_check -s exit:2 -e not-empty ed -s - <<'CMDS'
H
a
hello
.
.ka
'ad
'ap
CMDS
}

atf_test_case err_move_dest
err_move_dest_head()
{
	atf_set "descr" "Error: move to own range";
}
err_move_dest_body()
{

	atf_check -s exit:2 -e not-empty ed -s - <<'CMDS'
H
a
hello
world
.
1,$m1
CMDS
}

atf_test_case err_quit_addr
err_quit_addr_head()
{
	atf_set "descr" "Error: quit with address";
}
err_quit_addr_body()
{

	printf 'test\n' > input.txt
	atf_check -s exit:2 -e not-empty ed -s - <<'CMDS'
H
r input.txt
.q
CMDS
}

atf_test_case err_read_nofile
err_read_nofile_head()
{
	atf_set "descr" "Error: read nonexistent file";
}
err_read_nofile_body()
{

	atf_check -s exit:2 -e not-empty ed -s - <<'CMDS'
H
r a-good-book
CMDS
}

atf_test_case err_subst_delim
err_subst_delim_head()
{
	atf_set "descr" "Error: invalid substitute delimiter";
}
err_subst_delim_body()
{

	printf 'test\n' > input.txt
	atf_check -s exit:2 -e not-empty ed -s - <<'CMDS'
H
r input.txt
s . x
CMDS
}

atf_test_case err_subst_infinite
err_subst_infinite_head()
{
	atf_set "descr" "Error: infinite substitution loop";
}
err_subst_infinite_body()
{

	atf_check -s exit:2 -e not-empty ed -s - <<'CMDS'
H
a
a
.
s/x*/a/g
CMDS
}

atf_test_case err_subst_bracket
err_subst_bracket_head()
{
	atf_set "descr" "Error: unbalanced brackets in substitute";
}
err_subst_bracket_body()
{

	printf 'test\n' > input.txt
	atf_check -s exit:2 -e not-empty ed -s - <<'CMDS'
H
r input.txt
s/[xyx/a/
CMDS
}

atf_test_case err_subst_escape
err_subst_escape_head()
{
	atf_set "descr" "Error: invalid escape in substitute pattern";
}
err_subst_escape_body()
{

	printf 'test\n' > input.txt
	atf_check -s exit:2 -e not-empty ed -s - <<'CMDS'
H
r input.txt
s/\a\b\c/xyz/
CMDS
}

atf_test_case err_subst_empty
err_subst_empty_head()
{
	atf_set "descr" "Error: empty substitute pattern";
}
err_subst_empty_body()
{

	printf 'test\n' > input.txt
	atf_check -s exit:2 -e not-empty ed -s - <<'CMDS'
H
r input.txt
s//xyz/
CMDS
}

atf_test_case err_subst_bare
err_subst_bare_head()
{
	atf_set "descr" "Error: bare substitute without previous";
}
err_subst_bare_body()
{

	atf_check -s exit:2 -e not-empty ed -s - <<'CMDS'
H
s
CMDS
}

atf_test_case err_subst_sr
err_subst_sr_head()
{
	atf_set "descr" "Error: invalid sr suffix";
}
err_subst_sr_body()
{

	atf_check -s exit:2 -o ignore -e not-empty ed -s - <<'CMDS'
H
a
hello world
.
/./
sr
CMDS
}

atf_test_case err_subst_equiv
err_subst_equiv_head()
{
	atf_set "descr" "Error: invalid equivalence class in substitute";
}
err_subst_equiv_body()
{

	atf_check -s exit:2 -e not-empty ed -s - <<'CMDS'
H
a
hello
.
s/[h[=]/x/
CMDS
}

atf_test_case err_subst_class
err_subst_class_head()
{
	atf_set "descr" "Error: unterminated character class";
}
err_subst_class_body()
{

	atf_check -s exit:2 -e not-empty ed -s - <<'CMDS'
H
a
hello
.
s/[h[:]/x/
CMDS
}

atf_test_case err_subst_collate
err_subst_collate_head()
{
	atf_set "descr" "Error: invalid collation class";
}
err_subst_collate_body()
{

	atf_check -s exit:2 -e not-empty ed -s - <<'CMDS'
H
a
hello
.
s/[h[.]/x/
CMDS
}

atf_test_case err_transfer_suffix
err_transfer_suffix_head()
{
	atf_set "descr" "Error: invalid transfer suffix (tt)";
}
err_transfer_suffix_body()
{

	atf_check -s exit:2 -e not-empty ed -s - <<'CMDS'
H
tt
CMDS
}

atf_test_case err_transfer_addr
err_transfer_addr_head()
{
	atf_set "descr" "Error: invalid transfer address";
}
err_transfer_addr_body()
{

	printf 'test\n' > input.txt
	atf_check -s exit:2 -e not-empty ed -s - <<'CMDS'
H
r input.txt
t0;-1
CMDS
}

atf_test_case err_undo_addr
err_undo_addr_head()
{
	atf_set "descr" "Error: undo with address";
}
err_undo_addr_body()
{

	printf 'test\n' > input.txt
	atf_check -s exit:2 -e not-empty ed -s - <<'CMDS'
H
r input.txt
.u
CMDS
}

atf_test_case err_write_nopath
err_write_nopath_head()
{
	atf_set "descr" "Error: write to invalid path";
}
err_write_nopath_body()
{

	printf 'test\n' > input.txt
	atf_check -s exit:2 -e not-empty ed -s - <<'CMDS'
H
r input.txt
w /to/some/far-away/place
CMDS
}

atf_test_case err_write_suffix
err_write_suffix_head()
{
	atf_set "descr" "Error: invalid write suffix (ww)";
}
err_write_suffix_body()
{

	atf_check -s exit:2 -e not-empty ed -s - <<'CMDS'
H
ww.o
CMDS
}

atf_test_case err_write_flags
err_write_flags_head()
{
	atf_set "descr" "Error: invalid write flags (wqp)";
}
err_write_flags_body()
{

	atf_check -s exit:2 -e not-empty ed -s - <<'CMDS'
H
wqp w.o
CMDS
}

atf_test_case err_crypt_addr
err_crypt_addr_head()
{
	atf_set "descr" "Error: crypt with address";
}
err_crypt_addr_body()
{

	printf 'test\n' > input.txt
	atf_check -s exit:2 -e not-empty ed -s - <<'CMDS'
H
r input.txt
.x
CMDS
}

atf_test_case err_scroll
err_scroll_head()
{
	atf_set "descr" "Error: invalid scroll command";
}
err_scroll_body()
{

	atf_check -s exit:2 -e not-empty ed -s - <<'CMDS'
H
z
z
CMDS
}

# ---------------------------------------------------------------------------
# Registration
# ---------------------------------------------------------------------------
atf_init_test_cases()
{

	# Basic commands
	atf_add_test_case append
	atf_add_test_case address
	atf_add_test_case change
	atf_add_test_case delete
	atf_add_test_case insert
	atf_add_test_case join
	atf_add_test_case mark
	atf_add_test_case move
	atf_add_test_case transfer
	atf_add_test_case transfer_search
	atf_add_test_case undo

	# Global commands
	atf_add_test_case global_move
	atf_add_test_case global_change
	atf_add_test_case global_substitute
	atf_add_test_case global_undo
	atf_add_test_case global_copy
	atf_add_test_case inverse_global

	# Substitution
	atf_add_test_case subst_backreference
	atf_add_test_case subst_range
	atf_add_test_case subst_charclass

	# File operations
	atf_add_test_case edit_file
	atf_add_test_case edit_command
	atf_add_test_case edit_reread
	atf_add_test_case edit_lowercase
	atf_add_test_case read_command
	atf_add_test_case read_default
	atf_add_test_case read_file
	atf_add_test_case write_pipe
	atf_add_test_case quit
	atf_add_test_case shell_command

	# Newline handling
	atf_add_test_case newline_insert
	atf_add_test_case newline_search

	# Error tests
	atf_add_test_case err_append_suffix
	atf_add_test_case err_addr_out_of_range
	atf_add_test_case err_addr_negative
	atf_add_test_case err_bang_addr
	atf_add_test_case err_bang_double
	atf_add_test_case err_change_suffix
	atf_add_test_case err_change_zero
	atf_add_test_case err_delete_suffix
	atf_add_test_case err_edit_suffix
	atf_add_test_case err_edit_addr
	atf_add_test_case err_edit_nosuffix
	atf_add_test_case err_file_addr
	atf_add_test_case err_file_suffix
	atf_add_test_case err_global_delim
	atf_add_test_case err_global_empty
	atf_add_test_case err_global_incomplete
	atf_add_test_case err_help_addr
	atf_add_test_case err_insert_suffix
	atf_add_test_case err_insert_zero
	atf_add_test_case err_mark_upper
	atf_add_test_case err_mark_zero
	atf_add_test_case err_mark_ref
	atf_add_test_case err_move_dest
	atf_add_test_case err_quit_addr
	atf_add_test_case err_read_nofile
	atf_add_test_case err_subst_delim
	atf_add_test_case err_subst_infinite
	atf_add_test_case err_subst_bracket
	atf_add_test_case err_subst_escape
	atf_add_test_case err_subst_empty
	atf_add_test_case err_subst_bare
	atf_add_test_case err_subst_sr
	atf_add_test_case err_subst_equiv
	atf_add_test_case err_subst_class
	atf_add_test_case err_subst_collate
	atf_add_test_case err_transfer_suffix
	atf_add_test_case err_transfer_addr
	atf_add_test_case err_undo_addr
	atf_add_test_case err_write_nopath
	atf_add_test_case err_write_suffix
	atf_add_test_case err_write_flags
	atf_add_test_case err_crypt_addr
	atf_add_test_case err_scroll
}
