
atf_test_case stdin
stdin_head()
{
	atf_set "descr" "stdin functionality"
}

stdin_body()
{
	# no file after .so
	atf_check \
		-o inline:".so\n" \
		-e empty \
		-s exit:0 \
		soelim <<-EOF
.so
EOF

	# only space after .so
	atf_check \
		-o inline:".so  	\n" \
		-e empty \
		-s exit:0 \
		soelim <<-EOF
.so  	
EOF

	# explicit stdin
	atf_check \
		-o inline:".so\n" \
		-e empty \
		-s exit:0 \
		soelim - <<-EOF
.so
EOF

	atf_check \
		-o empty \
		-e inline:"soelim: can't open 'afile': No such file or directory\n" \
		-s exit:1 \
		soelim <<-EOF
.so afile
EOF

	atf_check \
		-o inline:".soafile\n" \
		-e empty \
		-s exit:0 \
		soelim <<-EOF
.soafile
EOF

	atf_check \
		-o empty \
		-e inline:"soelim: can't open 'afile': No such file or directory\n" \
		-s exit:1 \
		soelim -C <<-EOF
.soafile
EOF
}

atf_test_case files
files_head()
{
	atf_set "descr" "testing files"
}

files_body()
{
	atf_check \
		-o inline:"This is a test\n" \
		-e inline:"soelim: can't open 'nonexistingfile': No such file or directory\n" \
		-s exit:1 \
		soelim $(atf_get_srcdir)/nonexisting.in

	cp $(atf_get_srcdir)/basic .
	atf_check \
		-o file:$(atf_get_srcdir)/basic.out \
		-e empty \
		-s exit:0 \
		soelim $(atf_get_srcdir)/basic.in

	rm -f basic
	atf_check \
		-o file:$(atf_get_srcdir)/basic.out \
		-e empty \
		-s exit:0 \
		soelim -I$(atf_get_srcdir) $(atf_get_srcdir)/basic.in

	atf_check \
		-o file:$(atf_get_srcdir)/basic-with-space.out \
		-e empty \
		-s exit:0 \
		soelim -I$(atf_get_srcdir) $(atf_get_srcdir)/basic-with-space.in

}

atf_test_case errors
errors_head()
{
	atf_set "descr" "error handling"
}

errors_body()
{
	cp $(atf_get_srcdir)/basic .

	# A failure on one file is not hidden by a later success.
	atf_check \
		-o ignore \
		-e ignore \
		-s exit:1 \
		soelim doesnotexist $(atf_get_srcdir)/basic.in

	# A read error is not reported as an empty file.
	mkdir dir
	atf_check \
		-o empty \
		-e ignore \
		-s exit:1 \
		soelim dir

	# Write errors are not ignored.
	atf_check \
		-o empty \
		-e ignore \
		-s exit:1 \
		sh -c 'soelim basic.in > /dev/full'

	# Self-inclusion is bounded.
	printf '.so self\n' > self
	atf_check \
		-o empty \
		-e match:"too many nested includes" \
		-s exit:1 \
		soelim self
}

atf_test_case whitespace
whitespace_head()
{
	atf_set "descr" "white space after the file name"
}

whitespace_body()
{
	printf 'included\n' > inc
	printf '.so inc  \nnext\n' > trailing
	atf_check \
		-o inline:"included\nnext\n" \
		-e empty \
		-s exit:0 \
		soelim trailing
}

atf_init_test_cases()
{
	atf_add_test_case stdin
	atf_add_test_case files
	atf_add_test_case errors
	atf_add_test_case whitespace
}
