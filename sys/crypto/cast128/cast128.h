/*	$FreeBSD$	*/

#ifndef _CAST128_H_
#define _CAST128_H_

#include <opencrypto/cast.h>

#define cast128_key	cast_key

#define cast128_setkey(key, rawkey, keybytes) \
	cast_setkey((key), (rawkey), (keybytes))
#define cast128_encrypt(key, inblock, outblock) \
	cast_encrypt((key), (inblock), (outblock))
#define cast128_decrypt(key, inblock, outblock) \
	cast_decrypt((key), (inblock), (outblock))

#endif /* _CAST128_H_ */
