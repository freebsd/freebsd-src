BEGIN {
	text = "here is some text"
	repl = "<FOO&BAR \\q \\ \\\\ \\& \\\\& \\\\\\&>"
	printf "orig = \"%s\", repl = \"%s\"\n", text, repl
	sub(/some/, repl, text)
	printf "result is \"%s\"\n", text
}
