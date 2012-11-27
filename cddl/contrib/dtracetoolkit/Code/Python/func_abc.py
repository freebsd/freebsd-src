#!/usr/bin/python

import time

def func_c():
	print "Function C"	
	time.sleep(1)

def func_b():
	print "Function B"
	time.sleep(1)
	func_c()

def func_a():
	print "Function A"
	time.sleep(1)
	func_b()

func_a()
