#
# Yes, the two "cpio archive" formats *are* supposed to just be "short".
# The idea is to indicate archives produced on machines with the same
# byte order as the machine running "file" with "cpio archive", and
# to indicate archives produced on machines with the opposite byte order
# from the machine running "file" with "byte-swapped cpio archive".
#
# The SVR4 "cpio(4)" hints that there are additional formats, but they
# are defined as "short"s; I think all the new formats are
# character-header formats, and thus are strings not numbers.
#
0	short		070707		cpio archive
0	short		0143561		byte-swapped cpio archive
0	string		070707		ASCII cpio archive (pre-SVR4 or odc)
0	string		070701		ASCII cpio archive (SVR4 with no CRC)
0	string		070702		ASCII cpio archive (SVR4 with CRC)
