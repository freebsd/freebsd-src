#
# Sound formats, from Jan Nicolai Langfeldt <janl@ifi.uio.no>,
#

# Sun/NeXT audio data
0	string		.snd		audio data:
>12	belong		1		8-bit u-law,
>12	belong		2		8-bit linear PCM,
>12	belong		3		16-bit linear PCM,
>12	belong		4		24-bit linear PCM,
>12	belong		5		32-bit linear PCM,
>12	belong		6		32-bit floating point,
>12	belong		7		64-bit floating point,
>12	belong		23		compressed (G.721 ADPCM),
>20	belong		1		mono,
>20	belong		2		stereo,
>20	belong		4		quad,
>16	belong		x		%d Hz
# DEC systems (e.g. DECstation 5000) use a variant of the Sun/NeXT format
# that uses little-endian encoding and has a different magic number
# (0x0064732E in little-endian encoding).
0	lelong		0x0064732E	DEC audio data:
>12	lelong		1		8-bit u-law,
>12	lelong		2		8-bit linear PCM,
>12	lelong		3		16-bit linear PCM,
>12	lelong		4		24-bit linear PCM,
>12	lelong		5		32-bit linear PCM,
>12	lelong		6		32-bit floating point,
>12	lelong		7		64-bit floating point,
>12	lelong		23		compressed (G.721 ADPCM),
>20	lelong		1		mono,
>20	lelong		2		stereo,
>20	lelong		4		quad,
>16	lelong		x		%d Hz
# Bytes 0-3 of AIFF, AIFF-C, & 8SVX audio files are "FORM"
8	string		AIFF		AIFF audio data
8	string		AIFC		AIFF-C audio data
8	string		8SVX		IFF/8SVX audio data
# Bytes 0-3 of Waveform (*.wav) audio files are "RIFF"
8	string		WAVE		Waveform audio data
0	string		Creative\ Voice\ File	Soundblaster audio data
0	long		0x4e54524b	MultiTrack sound data file
>4	long		x		- version %ld
