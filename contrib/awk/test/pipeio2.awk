# From: megaadm@rina.quantum.de
# Subject: Bug report - closing down pipes which read from shell com
# To: bug-gnu-utils@prep.ai.mit.edu
# Date: Thu, 27 Feb 1997 23:19:16 +0100 (CET)
# CC: arnold@gnu.ai.mit.edu
# 
# Hello people,
# 
# i think i found a bug or something mysterious behaviour in
# gawk Version 3.0 patchlevel 0.
# 
# I am running on linux 2.0.25 under bash.
# 
# Could you please have a look at the following awk program
# an let me please know, if this is what i expect it to,
# namely a bug.
# 
# ----------- cut here --------------------------------------------
BEGIN	{
			# OS is linux 2.0.25
			# shell is bash
			# Gnu Awk (gawk) 3.0, patchlevel 0
			# The command i typed on the shell was "gawk -f <this_prog> -"

			#com = "cal 01 1997"
			com = ("cat " SRCDIR "/pipeio2.in")

			while ((com | getline fnam) > 0) {

				com_tr = "echo " fnam " | tr [0-9]. ..........."
				print "\'" com_tr "\'"

				com_tr | getline nam
				print nam

				# please run that program and take a look at the
				# output. I think this is what was expected.

				# Then comment in the following 4 lines and see
				# what happens. I expect the first pipe "com | getline"
				# not to be close, but i think this is exactly what happens
				# So, is this ok ?

				if (close(com_tr) < 0) {
					print ERRNO
					break
				}
			}

			close(com)
		}
# ----------- cut here --------------------------------------------
# 
# There is another thing i do not understand.
# Why doesn't the awk - command "close" reports an
# error, if i would say close("abc") which i had never
# openend ?
# 
# Regards,
# Ulrich Gvbel
# -- 
# /********************************************************\
# *     Ulrich Gvbel, goebel@quantum.de                    *
# *     Quantum Gesellschaft f|r Software mbH, Dortmund    *
# *     phone  : +49-231-9749-201  fax: +49-231-9749-3     *
# *     private: +49-231-803994    fax: +49-231-803994     *
# \********************************************************/
