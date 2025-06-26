
atf_test_case simple
atf_test_case unified
atf_test_case header
atf_test_case header_ns
atf_test_case ifdef
atf_test_case group_format
atf_test_case side_by_side
atf_test_case side_by_side_tabbed
atf_test_case brief_format
atf_test_case b230049
atf_test_case stripcr_o
atf_test_case b252515
atf_test_case b278988
atf_test_case Bflag
atf_test_case Nflag
atf_test_case tabsize
atf_test_case conflicting_format
atf_test_case label
atf_test_case report_identical
atf_test_case non_regular_file
atf_test_case binary
atf_test_case functionname
atf_test_case noderef
atf_test_case ignorecase
atf_test_case dirloop

simple_body()
{
	atf_check -o file:$(atf_get_srcdir)/simple.out -s eq:1 \
		diff "$(atf_get_srcdir)/input1.in" "$(atf_get_srcdir)/input2.in"

	atf_check -o file:$(atf_get_srcdir)/simple_e.out -s eq:1 \
		diff -e "$(atf_get_srcdir)/input1.in" "$(atf_get_srcdir)/input2.in"

	atf_check -o file:$(atf_get_srcdir)/simple_u.out -s eq:1 \
		diff -u -L input1 -L input2 "$(atf_get_srcdir)/input1.in" "$(atf_get_srcdir)/input2.in"

	atf_check -o file:$(atf_get_srcdir)/simple_n.out -s eq:1 \
		diff -n "$(atf_get_srcdir)/input1.in" "$(atf_get_srcdir)/input2.in"

	atf_check -o inline:"Files $(atf_get_srcdir)/input1.in and $(atf_get_srcdir)/input2.in differ\n" -s eq:1 \
		diff -q "$(atf_get_srcdir)/input1.in" "$(atf_get_srcdir)/input2.in"

	atf_check \
		diff -q "$(atf_get_srcdir)/input1.in" "$(atf_get_srcdir)/input1.in"

	atf_check -o file:$(atf_get_srcdir)/simple_i.out -s eq:1 \
		diff -i "$(atf_get_srcdir)/input_c1.in" "$(atf_get_srcdir)/input_c2.in"

	atf_check -o file:$(atf_get_srcdir)/simple_w.out -s eq:1 \
		diff -w "$(atf_get_srcdir)/input_c1.in" "$(atf_get_srcdir)/input_c2.in"

	atf_check -o file:$(atf_get_srcdir)/simple_b.out -s eq:1 \
		diff -b "$(atf_get_srcdir)/input_c1.in" "$(atf_get_srcdir)/input_c2.in"

	atf_check -o file:$(atf_get_srcdir)/simple_p.out -s eq:1 \
		diff --label input_c1.in --label input_c2.in -p "$(atf_get_srcdir)/input_c1.in" "$(atf_get_srcdir)/input_c2.in"
}

unified_body()
{
	atf_check -o file:$(atf_get_srcdir)/unified_p.out -s eq:1 \
		diff -up -L input_c1.in -L input_c2.in  "$(atf_get_srcdir)/input_c1.in" "$(atf_get_srcdir)/input_c2.in"
	atf_check -o file:$(atf_get_srcdir)/unified_9999.out -s eq:1 \
		diff -u9999 -L input_c1.in -L input_c2.in "$(atf_get_srcdir)/input_c1.in" "$(atf_get_srcdir)/input_c2.in"
}

b230049_body()
{
	printf 'a\nb\r\nc\n' > b230049_a.in
	printf 'a\r\nb\r\nc\r\n' > b230049_b.in
	atf_check -o empty -s eq:0 \
		diff -up --strip-trailing-cr -L b230049_a.in -L b230049_b.in \
		    b230049_a.in b230049_b.in
}

stripcr_o_body()
{
	printf 'a\nX\nc\n' > stripcr_o_X.in
	printf 'a\r\nY\r\nc\r\n' > stripcr_o_Y.in
	atf_check -o "file:$(atf_get_srcdir)/strip_o.out" -s eq:1 \
		diff -L1 -L2 -u --strip-trailing-cr stripcr_o_X.in stripcr_o_Y.in
}

b252515_body()
{
	printf 'a b\n' > b252515_a.in
	printf 'a  b\n' > b252515_b.in
	atf_check -o empty -s eq:0 \
		diff -qw b252515_a.in b252515_b.in
}

b278988_body()
{
	printf 'a\nb\nn' > b278988.a.in
	printf 'a\n\nb\nn' > b278988.b.in
	atf_check -o empty -s eq:0 \
		diff -Bw b278988.a.in b278988.b.in
}

header_body()
{
	export TZ=UTC
	: > empty
	echo hello > hello
	touch -d 2015-04-03T01:02:03 empty
	touch -d 2016-12-22T11:22:33 hello
	atf_check -o "file:$(atf_get_srcdir)/header.out" -s eq:1 \
		diff -u empty hello
}

header_ns_body()
{
	export TZ=UTC
	: > empty
	echo hello > hello
	touch -d 2015-04-03T01:02:03.123456789 empty
	touch -d 2016-12-22T11:22:33.987654321 hello
	atf_check -o "file:$(atf_get_srcdir)/header_ns.out" -s eq:1 \
		diff -u empty hello
}

ifdef_body()
{
	atf_check -o file:$(atf_get_srcdir)/ifdef.out -s eq:1 \
		diff -D PLOP "$(atf_get_srcdir)/input_c1.in" \
		"$(atf_get_srcdir)/input_c2.in"
}

group_format_body()
{
	atf_check -o file:$(atf_get_srcdir)/group-format.out -s eq:1 \
		diff --changed-group-format='<<<<<<< (local)
%<=======
%>>>>>>>> (stock)
' "$(atf_get_srcdir)/input_c1.in" "$(atf_get_srcdir)/input_c2.in"
}

side_by_side_body()
{
	atf_check -o save:A printf "A\nB\nC\n"
	atf_check -o save:B printf "D\nB\nE\n"

	exp_output=$(printf "A[[:space:]]+|[[:space:]]+D\nB[[:space:]]+B\nC[[:space:]]+|[[:space:]]+E")
	exp_output_suppressed=$(printf "A[[:space:]]+|[[:space:]]+D\nC[[:space:]]+|[[:space:]]+E")

	atf_check -o match:"$exp_output" -s exit:1 \
	    diff --side-by-side A B
	atf_check -o match:"$exp_output" -s exit:1 \
	    diff -y A B
	atf_check -o match:"$exp_output_suppressed" -s exit:1 \
	    diff -y --suppress-common-lines A B
	atf_check -o match:"$exp_output_suppressed" -s exit:1 \
	    diff -W 65 -y --suppress-common-lines A B
}

side_by_side_tabbed_body()
{
	file_a=$(atf_get_srcdir)/side_by_side_tabbed_a.in
	file_b=$(atf_get_srcdir)/side_by_side_tabbed_b.in

	atf_check -o save:diffout -s not-exit:0 \
	    diff -y ${file_a} ${file_b}
	atf_check -o save:diffout_expanded -s not-exit:0 \
	    diff -yt ${file_a} ${file_b}

	atf_check -o not-empty grep -Ee 'file A.+file B' diffout
	atf_check -o not-empty grep -Ee 'file A.+file B' diffout_expanded

	atf_check -o not-empty grep -Ee 'tabs.+tabs' diffout
	atf_check -o not-empty grep -Ee 'tabs.+tabs' diffout_expanded
}

brief_format_body()
{
	atf_check mkdir A B

	atf_check -x "echo 1 > A/test-file"
	atf_check -x "echo 2 > B/test-file"

	atf_check cp -Rf A C
	atf_check cp -Rf A D

	atf_check -x "echo 3 > D/another-test-file"

	atf_check \
	    -s exit:1 \
	    -o inline:"Files A/test-file and B/test-file differ\n" \
	    diff -rq A B

	atf_check diff -rq A C

	atf_check \
	    -s exit:1 \
	    -o inline:"Only in D: another-test-file\n" \
	    diff -rq A D

	atf_check \
	    -s exit:1 \
	    -o inline:"Files A/another-test-file and D/another-test-file differ\n" \
	    diff -Nrq A D
}

Bflag_body()
{
	atf_check -x 'printf "A\nB\n" > A'
	atf_check -x 'printf "A\n\nB\n" > B'
	atf_check -x 'printf "A\n \nB\n" > C'
	atf_check -x 'printf "A\nC\nB\n" > D'
	atf_check -x 'printf "A\nB\nC\nD\nE\nF\nG\nH" > E'
	atf_check -x 'printf "A\n\nB\nC\nD\nE\nF\nX\nH" > F'

	atf_check -s exit:0 -o inline:"" diff -B A B
	atf_check -s exit:1 -o file:"$(atf_get_srcdir)/Bflag_C.out" diff -B A C
	atf_check -s exit:1 -o file:"$(atf_get_srcdir)/Bflag_D.out" diff -B A D
	atf_check -s exit:1 -o file:"$(atf_get_srcdir)/Bflag_F.out" diff -B E F
}

Nflag_body()
{
	atf_check -x 'printf "foo" > A'

	atf_check -s exit:1 -o ignore -e ignore diff -N A NOFILE 
	atf_check -s exit:1 -o ignore -e ignore diff -N NOFILE A 
	atf_check -s exit:2 -o ignore -e ignore diff -N NOFILE1 NOFILE2 
}

tabsize_body()
{
	printf "\tA\n" > A
	printf "\tB\n" > B

	atf_check -s exit:1 \
	    -o inline:"1c1\n<  A\n---\n>  B\n" \
	    diff -t --tabsize 1 A B
}

conflicting_format_body()
{
	printf "\tA\n" > A
	printf "\tB\n" > B

	atf_check -s exit:2 -e ignore diff -c -u A B
	atf_check -s exit:2 -e ignore diff -e -f A B
	atf_check -s exit:2 -e ignore diff -y -q A B
	atf_check -s exit:2 -e ignore diff -q -u A B
	atf_check -s exit:2 -e ignore diff -q -c A B
	atf_check -s exit:2 -e ignore diff --normal -c A B
	atf_check -s exit:2 -e ignore diff -c --normal A B

	atf_check -s exit:1 -o ignore -e ignore diff -u -u A B
	atf_check -s exit:1 -o ignore -e ignore diff -e -e A B
	atf_check -s exit:1 -o ignore -e ignore diff -y -y A B
	atf_check -s exit:1 -o ignore -e ignore diff -q -q A B
	atf_check -s exit:1 -o ignore -e ignore diff -c -c A B
	atf_check -s exit:1 -o ignore -e ignore diff --normal --normal A B
}

label_body()
{
	printf "\tA\n" > A

	atf_check -o inline:"Files hello and world are identical\n" \
		-s exit:0 diff --label hello --label world -s A A

	atf_check -o inline:"Binary files hello and world differ\n" \
		-s exit:1 diff --label hello --label world `which diff` `which ls`
}

report_identical_head()
{
	atf_set "require.user" unprivileged
}
report_identical_body()
{
	printf "\tA\n" > A
	printf "\tB\n" > B
	atf_check -s exit:0 -o match:"are identical" \
		  diff -s A A
	atf_check -s exit:1 -o not-match:"are identical" \
		  diff -s A B
	chmod -r B
	atf_check -s exit:2 -e inline:"diff: B: Permission denied\n" \
		-o empty diff -s A B
}

non_regular_file_body()
{
	printf "\tA\n" > A
	mkfifo B
	printf "\tA\n" > B &

	atf_check diff A B
	printf "\tB\n" > B &
	atf_check -s exit:1 \
		-o inline:"--- A\n+++ B\n@@ -1 +1 @@\n-\tA\n+\tB\n" \
		diff --label A --label B -u A B
}

binary_body()
{
	# the NUL byte has to be after at least BUFSIZ bytes to trick asciifile()
	yes 012345678901234567890123456789012345678901234567890 | head -n 174 > A
	cp A B
	printf '\n\0\n' >> A
	printf '\nx\n' >> B

	atf_check -o inline:"Binary files A and B differ\n" -s exit:1 diff A B
	atf_check -o inline:"176c\nx\n.\n" -s exit:1 diff -ae A B
}

functionname_body()
{
	atf_check -o file:$(atf_get_srcdir)/functionname_c.out -s exit:1 \
		diff -u -p -L functionname.in -L functionname_c.in \
		"$(atf_get_srcdir)/functionname.in" "$(atf_get_srcdir)/functionname_c.in"

	atf_check -o file:$(atf_get_srcdir)/functionname_objcm.out -s exit:1 \
		diff -u -p -L functionname.in -L functionname_objcm.in \
		"$(atf_get_srcdir)/functionname.in" "$(atf_get_srcdir)/functionname_objcm.in"

	atf_check -o file:$(atf_get_srcdir)/functionname_objcclassm.out -s exit:1 \
		diff -u -p -L functionname.in -L functionname_objcclassm.in \
		"$(atf_get_srcdir)/functionname.in" "$(atf_get_srcdir)/functionname_objcclassm.in"
}

noderef_body()
{
	atf_check mkdir A B

	atf_check -x "echo 1 > A/test-file"
	atf_check -x "echo 1 > test-file"
	atf_check -x "echo 1 > test-file2"

	atf_check ln -s $(pwd)/test-file B/test-file

	atf_check -o empty -s exit:0 diff -r A B
	atf_check -o inline:"File A/test-file is a file while file B/test-file is a symbolic link\n" \
		-s exit:1 diff -r --no-dereference A B

	# both test files are now the same symbolic link
	atf_check rm A/test-file

	atf_check ln -s $(pwd)/test-file A/test-file
	atf_check -o empty -s exit:0 diff -r A B
	atf_check -o empty -s exit:0 diff -r --no-dereference A B

	# make test files different symbolic links, but same contents
	atf_check unlink A/test-file
	atf_check ln -s $(pwd)/test-file2 A/test-file

	atf_check -o empty -s exit:0 diff -r A B
	atf_check -o inline:"Symbolic links A/test-file and B/test-file differ\n" -s exit:1 diff -r --no-dereference A B
}

ignorecase_body()
{
	atf_check mkdir A
	atf_check mkdir B

	atf_check -x "echo hello > A/foo"
	atf_check -x "echo hello > B/FOO"

	atf_check -o empty -s exit:0 diff -u -r --ignore-file-name-case A B
}

dirloop_head()
{
	atf_set "timeout" "10"
}
dirloop_body()
{
	atf_check mkdir -p a/foo/bar
	atf_check ln -s .. a/foo/bar/up
	atf_check cp -a a b
	atf_check \
	    -e match:"a/foo/bar/up: Directory loop detected" \
	    -e match:"b/foo/bar/up: Directory loop detected" \
	    diff -r a b
}

atf_init_test_cases()
{
	atf_add_test_case simple
	atf_add_test_case unified
	atf_add_test_case header
	atf_add_test_case header_ns
	atf_add_test_case ifdef
	atf_add_test_case group_format
	atf_add_test_case side_by_side
	atf_add_test_case side_by_side_tabbed
	atf_add_test_case brief_format
	atf_add_test_case b230049
	atf_add_test_case stripcr_o
	atf_add_test_case b252515
	atf_add_test_case b278988
	atf_add_test_case Bflag
	atf_add_test_case Nflag
	atf_add_test_case tabsize
	atf_add_test_case conflicting_format
	atf_add_test_case label
	atf_add_test_case report_identical
	atf_add_test_case non_regular_file
	atf_add_test_case binary
	atf_add_test_case functionname
	atf_add_test_case noderef
	atf_add_test_case ignorecase
	atf_add_test_case dirloop
}
