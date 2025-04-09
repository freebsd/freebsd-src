# SPDX-License-Identifier: ISC
#
# Copyright (c) 2025 Lexi Winter
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

atf_test_case "basic" "cleanup"
basic_head()
{
	cat >input.1 <<END
this is the first input file
it has multiple lines
END

	cat >input.2 <<END
here lies the second input file
some lines

are empty
END

	cat >input.3 <<END
third of the input files am i
and i have
more
lines
than before
END

	cat >expected <<END
this is the first input file	are empty			lines
it has multiple lines		third of the input files am i	than before
here lies the second input file	and i have
some lines			more
END
}

basic_body()
{
	column -c120 input.1 input.2 input.3 >output
	atf_check -s exit:0 diff expected output
}

basic_cleanup()
{
	rm -f input.1 input.2 input.3 expected output
}

atf_test_case "rows" "cleanup"
rows_head()
{
	cat >input.1 <<END
this is the first input file
it has multiple lines
END

	cat >input.2 <<END
here lies the second input file
some lines

are empty
END

	cat >input.3 <<END
third of the input files am i
and i have
more
lines
than before
END

	cat >expected <<END
this is the first input file	it has multiple lines		here lies the second input file
some lines			are empty			third of the input files am i
and i have			more				lines
than before
END
}

rows_body()
{
	column -xc120 input.1 input.2 input.3 >output
	atf_check -s exit:0 diff expected output
}

rows_cleanup()
{
	rm -f input.1 input.2 input.3 expected output
}

atf_test_case "basic_table" "cleanup"
basic_table_head()
{
	cat >input.1 <<END
1 2 3 4
foo bar baz quux
END

	cat >input.2 <<END
fie fi fo fum
END

	cat >input.3 <<END
where did my
fields go
argh
END

	cat >expected <<END
1       2    3    4
foo     bar  baz  quux
fie     fi   fo   fum
where   did  my
fields  go
argh
END
}

basic_table_body()
{
	column -tc120 input.1 input.2 input.3 >output
	atf_check -s exit:0 diff expected output
}

basic_table_cleanup()
{
	rm -f input.1 input.2 input.3 expected output
}

atf_test_case "colonic_table" "cleanup"
colonic_table_head()
{
	cat >input <<END
one:two.three
four.five:six
seven.:eight.:nine
:ein
::zwei
drei..
vier:
:

END

	cat >expected <<END
one    two    three
four   five   six
seven  eight  nine
ein
zwei
drei
vier

END
}

colonic_table_body()
{
	column -tc120 -s:. input >output
	atf_check -s exit:0 diff expected output
}

colonic_table_cleanup()
{
	rm -f input expected output
}

atf_init_test_cases()
{
	atf_add_test_case basic
	atf_add_test_case rows
	atf_add_test_case basic_table
	atf_add_test_case colonic_table
}
