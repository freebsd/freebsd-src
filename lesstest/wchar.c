#include <unistd.h>
#include "lt_types.h"

// Return number of bytes in the UTF-8 sequence which begins with a given byte.
int wchar_len(byte b) {
	if ((b & 0xE0) == 0xC0) return 2;
	if ((b & 0xF0) == 0xE0) return 3;
	if ((b & 0xF8) == 0xF0) return 4;
	return 1;
}

void store_wchar(byte** p, wchar ch) {
	if (ch < 0x80) {
		*(*p)++ = (char) ch;
	} else if (ch < 0x800) {
		*(*p)++ = (byte) (0xC0 | ((ch >> 6) & 0x1F));
		*(*p)++ = (byte) (0x80 | (ch & 0x3F));
	} else if (ch < 0x10000) {
		*(*p)++ = (byte) (0xE0 | ((ch >> 12) & 0x0F));
		*(*p)++ = (byte) (0x80 | ((ch >> 6) & 0x3F));
		*(*p)++ = (byte) (0x80 | (ch & 0x3F));
	} else {
		*(*p)++ = (byte) (0xF0 | ((ch >> 18) & 0x07));
		*(*p)++ = (byte) (0x80 | ((ch >> 12) & 0x3F));
		*(*p)++ = (byte) (0x80 | ((ch >> 6) & 0x3F));
		*(*p)++ = (byte) (0x80 | (ch & 0x3F));
	}
}

wchar load_wchar(const byte** p) {
	wchar ch;
	switch (wchar_len(**p)) {
	default:
		ch = *(*p)++ & 0xFF;
		break;
	case 2:
		ch = (*(*p)++ & 0x1F) << 6;
		ch |= *(*p)++ & 0x3F;
		break;
	case 3:
		ch = (*(*p)++ & 0x0F) << 12;
		ch |= (*(*p)++ & 0x3F) << 6;
		ch |= (*(*p)++ & 0x3F);
		break;
	case 4:
		ch = (*(*p)++ & 0x07) << 18;
		ch |= (*(*p)++ & 0x3F) << 12;
		ch |= (*(*p)++ & 0x3F) << 6;
		ch |= (*(*p)++ & 0x3F);
		break;
	}
	return ch;
}

wchar read_wchar(int fd) {
	byte cbuf[UNICODE_MAX_BYTES];
	int n = read(fd, &cbuf[0], 1);
	if (n <= 0)
		return 0;
	int len = wchar_len(cbuf[0]);
	int i;
	for (i = 1; i < len; ++i) {
		int n = read(fd, &cbuf[i], 1);
		if (n != 1) return 0;
	}
	const byte* cp = cbuf;
	wchar ch = load_wchar(&cp);
	// assert(cp-cbuf == len);
	return ch;
}
