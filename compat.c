/* Carsten Kunze, 2016 */

#include <string.h>

#ifndef HAVE_STRLCPY
size_t
strlcpy(char *dst, const char *src, size_t dstsize) {
	size_t srcsize;
	/* Not conform to strlcpy, but avoids to access illegal memory in case
	 * of unterminated strings */
	for (srcsize = 0; srcsize < dstsize; srcsize++)
		if (!src[srcsize])
			break;
	if (dstsize > srcsize)
		dstsize = srcsize;
	else if (dstsize)
		dstsize--;
	if (dstsize)
		/* assumes non-overlapping buffers */
		memcpy(dst, src, dstsize);
	dst[dstsize] = 0;
	return srcsize;
}
#endif

#ifndef HAVE_STRLCAT
size_t
strlcat(char *dst, const char *src, size_t dstsize) {
	size_t ld, ls;
	for (ld = 0; ld < dstsize - 1; ld++)
		if (!dst[ld])
			break;
	dst += ld;
	dstsize -= ld;
	for (ls = 0; ls < dstsize; ls++)
		if (!src[ls])
			break;
	if (dstsize > ls)
		dstsize = ls;
	else if (dstsize)
		dstsize--;
	if (dstsize)
		memcpy(dst, src, dstsize);
	dst[dstsize] = 0;
	return ld + ls;
}
#endif
