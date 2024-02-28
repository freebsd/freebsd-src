#!/bin/sh

echo T.csv: tests of csv field splitting, no embedded newlines

awk=${awk-../a.out}

$awk '
BEGIN {
	FS = "\t"
	awk = "../a.out --csv"
}
NF == 0 || $1 ~ /^#/ {
	next
}
$1 ~ /try/ {	# new test
	nt++
	sub(/try /, "")
	prog = $0
	printf("%3d  %s\n", nt, prog)
	prog = sprintf("%s '"'"'%s'"'"'", awk, prog)
	# print "prog is", prog
	nt2 = 0
	while (getline > 0) {
		if (NF == 0)	# blank line terminates a sequence
			break
		input = $1
		for (i = 2; i < NF; i++)	# input data
			input = input "\t" $i
		test = sprintf("./echo '"'"'%s'"'"' | %s >foo1; ",
			input, prog)
		if ($NF == "\"\"")
			output = ">foo2;"
		else
			output = sprintf("./echo '"'"'%s'"'"' >foo2; ", $NF)
		gsub(/\\t/, "\t", output)
		gsub(/\\n/, "\n", output)
		run = sprintf("cmp foo1 foo2 || echo test %d.%d failed",
			nt, ++nt2)
		# print  "input is", input
		# print  "test is", test
		# print  "output is", output
		# print  "run is", run
		system(test output run)
	}
	tt += nt2
}
END { print tt, "tests" }
' <<\!!!!
# General format:
# try program as rest of line
# $1	$2	$3	output1  (\t for tab, \n for newline,
# $1	$2	$3	output2  ("" for null)
# ... terminated by blank line


try  { for (i=1; i<=NF; i++) printf("[%s]", $i); printf("\n") }
a	[a]
  a	[  a]
,a	[][a]
 , a	[ ][ a]
a,b	[a][b]
a,b,c	[a][b][c]
""	[]
"abc"	[abc]
"a""b"	[a"b]
"a","b"	[a][b]
a""b	[a""b]
"a,b"	[a,b]
""""	["]
""""""	[""]
"""x"""	["x"]
""","""	[","]
,,""	[][][]
a""b	[a""b]
a''b	[a''b]
,,	[][][]
a,	[a][]
"",	[][]
,	[][]
!!!!
