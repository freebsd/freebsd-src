function foo() {
	return "aaaaaab"
}

BEGIN { 
	print match(foo(), "b")
}

{
	print match(substr($0, 1), "b")     
}
