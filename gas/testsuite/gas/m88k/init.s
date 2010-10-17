; Test proper padding of the .init section
	section	 .init,"x"
	align	 4
	subu	 r31,r31,16
	st	 r13,r31,32
