# tests for assigning to a function within that function

#1 - should be bad
function test1 (r) { gsub(r, "x", test1) }
BEGIN { test1("") }

#2 - should be bad
function test2 () { gsub(/a/, "x", test2) }
BEGIN { test2() }

#3 - should be ok
function test3 (r) { gsub(/a/, "x", r) }
BEGIN { test3("") }
