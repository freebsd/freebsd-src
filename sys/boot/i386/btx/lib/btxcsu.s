#
# Copyright (c) 1998 Robert Nordier
# All rights reserved.
#
# Redistribution and use in source and binary forms are freely
# permitted provided that the above copyright notice and this
# paragraph and the following disclaimer are duplicated in all
# such forms.
#
# This software is provided "AS IS" and without any express or
# implied warranties, including, without limitation, the implied
# warranties of merchantability and fitness for a particular
# purpose.
#

#	$Id:$

#
# BTX C startup code (ELF).
#

#
# Globals.
#
		.global _start
#
# Constants.
#
		.set ARGSIZ,0x60		# Size of arguments
#
# Client entry point.
#
_start: 	movl %eax,__base		# Set base address
		subl $ARGSIZ,%esp		# Set argument
		movl %esp,__args		#  pointer
		call main			# Invoke client main()
		call exit			# Invoke client exit()
#
# Data.
#
		.comm __base,4			# Client base address
		.comm __args,4			# Client arguments
