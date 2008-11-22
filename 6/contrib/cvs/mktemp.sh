# mktemp

# Copyright (c) Derek Price, Ximbiot <http://ximbiot.com>, and the
# Free Software Foundation, Inc. <http://gnu.org>

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
# 02111-1307, USA.



# This Bourne Shell scriptlet is intended as a simple replacement for
# the BSD mktemp function for systems that do not support mktemp.  It
# currently does not check that the files it is creating did not exist
# previously and it does not verify that it successfully creates the
# files it returns the names of.
mktemp() {
	if test x"$1" = x-d; then
		tmp=`echo $2 |sed "s/XXXXXX/$$/"`
		(umask 077 && exec mkdir $tmp) || return 1
	else
		tmp=`echo $1 |sed "s/XXXXXX/$$/"`
		(umask 077 && touch $tmp) || return 1
	fi
	echo $tmp
	return 0
}
		
