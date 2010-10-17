#!/bin/sh
# objdump the symbol table, but strip off the headings and symbol
# numbers and sort the result.  Intended for use in comparing symbol
# tables that may not be in the same order.

objdump +symbols +omit-symbol-numbers $1 \
	| sort
#eof
