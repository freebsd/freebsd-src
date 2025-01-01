#/*
# *  Copyright (C) 2017 - This file is part of libecc project
# *
# *  Authors:
# *      Ryad BENADJILA <ryadbenadjila@gmail.com>
# *      Arnaud EBALARD <arnaud.ebalard@ssi.gouv.fr>
# *      Jean-Pierre FLORI <jean-pierre.flori@ssi.gouv.fr>
# *
# *  Contributors:
# *      Nicolas VIVET <nicolas.vivet@ssi.gouv.fr>
# *      Karim KHALFALLAH <karim.khalfallah@ssi.gouv.fr>
# *
# *  This software is licensed under a dual BSD and GPL v2 license.
# *  See LICENSE file at the root folder of the project.
# */
import struct, sys

keccak_rc = [
        0x0000000000000001, 0x0000000000008082, 0x800000000000808A, 0x8000000080008000,
        0x000000000000808B, 0x0000000080000001, 0x8000000080008081, 0x8000000000008009,
        0x000000000000008A, 0x0000000000000088, 0x0000000080008009, 0x000000008000000A,
        0x000000008000808B, 0x800000000000008B, 0x8000000000008089, 0x8000000000008003,
        0x8000000000008002, 0x8000000000000080, 0x000000000000800A, 0x800000008000000A,
        0x8000000080008081, 0x8000000000008080, 0x0000000080000001, 0x8000000080008008
]

keccak_rot = [
        [  0, 36,  3, 41, 18 ],
        [  1, 44, 10, 45,  2 ],
        [ 62,  6, 43, 15, 61 ],
        [ 28, 55, 25, 21, 56 ],
        [ 27, 20, 39,  8, 14 ],
]

def is_python_2():
    if sys.version_info[0] < 3:
        return True
    else:
        return False

# Keccak function
def keccak_rotl(x, l):
	return (((x << l) ^ (x >> (64 - l))) & (2**64-1))

def keccakround(bytestate, rc):
	# Import little endian state
	state = [0] * 25
	for i in range(0, 25):
		to_unpack = ''.join(bytestate[(8*i):(8*i)+8])
		if is_python_2() == False:
			to_unpack = to_unpack.encode('latin-1')
		(state[i],) = struct.unpack('<Q', to_unpack)
	# Proceed with the KECCAK core
	bcd = [0] * 25
	# Theta
	for i in range(0, 5):
		bcd[i] = state[i] ^ state[i + (5*1)] ^ state[i + (5*2)] ^ state[i + (5*3)] ^ state[i + (5*4)] 	
	
	for i in range(0, 5):
		tmp = bcd[(i+4)%5] ^ keccak_rotl(bcd[(i+1)%5], 1)
		for j in range(0, 5):
			state[i + (5 * j)] = state[i + (5 * j)] ^ tmp
	# Rho and Pi
	for i in range(0, 5):
		for j in range(0, 5):
			bcd[j + (5*(((2*i)+(3*j)) % 5))] = keccak_rotl(state[i + (5*j)], keccak_rot[i][j])
	# Chi
	for i in range(0, 5):
		for j in range(0, 5):
			state[i + (5*j)] = bcd[i + (5*j)] ^ (~bcd[((i+1)%5) + (5*j)] & bcd[((i+2)%5) + (5*j)])
	# Iota
	state[0] = state[0] ^ keccak_rc[rc]
	# Pack the output state
	output = [0] * (25 * 8)
	for i in range(0, 25):
		packed = struct.pack('<Q', state[i])
		if is_python_2() == True:
			output[(8*i):(8*i)+1] = packed
		else:
			output[(8*i):(8*i)+1] = packed.decode('latin-1')
	return output

def keccakf(bytestate):
	for rnd in range(0, 24):
		bytestate = keccakround(bytestate, rnd)	
	return bytestate

# SHA-3 context class
class Sha3_ctx(object):
	def __init__(self, digest_size):
		self.digest_size = digest_size / 8
		self.block_size = (25*8) - (2 * (digest_size / 8))
		self.idx = 0
		self.state = [chr(0)] * (25 * 8)
	def digest_size(self):
		return self.digest_size
	def block_size(self):
		return self.block_size
	def update(self, message):
		if (is_python_2() == False):
			message = message.decode('latin-1')
		for i in range(0, len(message)):
			self.state[self.idx] = chr(ord(self.state[self.idx]) ^ ord(message[i]))
			self.idx = self.idx + 1
			if (self.idx == self.block_size):
				self.state = keccakf(self.state)
				self.idx = 0
	def digest(self):
		self.state[self.idx] = chr(ord(self.state[self.idx]) ^ 0x06)
		self.state[int(self.block_size - 1)] = chr(ord(self.state[int(self.block_size - 1)]) ^ 0x80)
		self.state = keccakf(self.state)
		digest = ''.join(self.state[:int(self.digest_size)])
		if is_python_2() == False:
			digest = digest.encode('latin-1')
		return digest
