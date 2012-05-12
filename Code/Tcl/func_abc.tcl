#!./tclsh

proc func_c {} {
	puts "Function C"
	after 1000
}

proc func_b {} {
	puts "Function B"
	after 1000
	func_c
}

proc func_a {} {
	puts "Function A"
	after 1000
	func_b
}

func_a
