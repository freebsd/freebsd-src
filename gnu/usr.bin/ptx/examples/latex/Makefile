# Example of using ptx with latex.
# Copyright (C) 1993 Free Software Foundation, Inc.
# Francois Pinard <pinard@iro.umontreal.ca>, 1993.

PTX = ../ptx
PTX_OPTIONS = -AfTWi.i

try: latex.dvi
	xdvi latex

latex.dvi: latex.tex table.tex
	latex latex

table.tex: Makefile ../COPYING
	$(PTX) $(PTX_OPTIONS) ../COPYING | sed 's///' > table.tex
