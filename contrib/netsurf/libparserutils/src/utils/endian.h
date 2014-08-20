/*
 * This file is part of LibParserUtils.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2009 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef parserutils_endian_h_
#define parserutils_endian_h_

static inline bool endian_host_is_le(void)
{
	static uint32_t magic = 0x10000002;

	return (((uint8_t *) &magic)[0] == 0x02);
}

static inline uint32_t endian_swap(uint32_t val)
{
	return ((val & 0xff000000) >> 24) | ((val & 0x00ff0000) >> 8) |
		((val & 0x0000ff00) << 8) | ((val & 0x000000ff) << 24);
}

static inline uint32_t endian_host_to_big(uint32_t host)
{
	if (endian_host_is_le())
		return endian_swap(host);

	return host;
}

static inline uint32_t endian_big_to_host(uint32_t big)
{
	if (endian_host_is_le())
		return endian_swap(big);

	return big;
}

#endif
