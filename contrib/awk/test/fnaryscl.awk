BEGIN {
	foo[1] = 4
	f1(foo)
}

function f1(a) { f2(a) }

function f2(b) { f3(b) }

function f3(c) { c = 6 }
