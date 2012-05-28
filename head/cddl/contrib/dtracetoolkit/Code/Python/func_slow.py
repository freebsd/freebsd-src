#!/usr/bin/python

def func_c():
	print "Function C"
	i = 0
	while (i < 3000000):
		i = i + 1
		j = i + 1

def func_b():
	print "Function B"
	i = 0
	while (i < 2000000):
		i = i + 1
		j = i + 1
	func_c()

def func_a():
	print "Function A"
	i = 0
	while (i < 1000000):
		i = i + 1
		j = i + 1
	func_b()

func_a()
