#!./tclsh

proc func_c {} {
	puts "Function C"
	set i 0
	while {$i < 300000} {
		set i [expr $i + 1]
	}
}

proc func_b {} {
	puts "Function B"
	set i 0
	while {$i < 200000} {
		set i [expr $i + 1]
	}
	func_c
}

proc func_a {} {
	puts "Function A"
	set i 0
	while {$i < 100000} {
		set i [expr $i + 1]
	}
	func_b
}

func_a
