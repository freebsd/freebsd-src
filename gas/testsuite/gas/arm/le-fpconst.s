# Test fp constants.
# These need ARM specific support because 8 byte fp constants in little
# endian mode are represented abnormally.
	
	.text
	.float 1.1
	.float 0
	.double 1.1
