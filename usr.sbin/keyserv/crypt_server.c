/*
 * Copyright (c) 1996
 *	Bill Paul <wpaul@ctr.columbia.edu>.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$Id: crypt_server.c,v 1.15 1996/12/25 19:21:10 wpaul Exp $
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>
#include <stdlib.h>
#include <dirent.h>
#include <err.h>
#include <rpc/des_crypt.h>
#include <rpc/des.h>
#include <string.h>
#include <dlfcn.h>
#include "crypt.h"

#ifndef lint
static const char rcsid[] = "$Id: crypt_server.c,v 1.15 1996/12/25 19:21:10 wpaul Exp $";
#endif

/*
 * The U.S. government stupidly believes that a) it can keep strong
 * crypto code a secret and b) that doing so somehow protects national
 * interests. It's wrong on both counts, but until it listens to reason
 * we have to make certain compromises so it doesn't have an excuse to
 * throw us in federal prison.
 *
 * Consequently, the core OS ships without DES support, and keyserv
 * defaults to using RC4 with only a 40 bit key, just like nutscrape.
 * This breaks compatibility with Secure RPC on other systems, but it
 * allows Secure RPC to work between FreeBSD systems that don't have the
 * DES package installed without throwing security totally out the window.
 *
 * In order to avoid having to supply two versions of keyserv (one with
 * DES and one without), we use dlopen() and friends to load libdes.so
 * into our address space at runtime. We check for the presence of
 * /usr/lib/libdes.so.3.0 at startup and load it if we find it. If we
 * can't find it, or the __des_crypt symbol doesn't exist, we fall back
 * to the RC4 encryption code. The user can specify another path using
 * the -p flag.
 */

 /* rc4.h */
typedef struct rc4_key
{      
   unsigned char state[256];       
   unsigned char x;        
   unsigned char y;
} rc4_key;

static void prepare_key(unsigned char *key_data_ptr,int key_data_len,
		 rc4_key *key);
static void rc4(unsigned char *buffer_ptr,int buffer_len,rc4_key * key);
static void swap_byte(unsigned char *a, unsigned char *b);

static void prepare_key(unsigned char *key_data_ptr, int key_data_len,
		 rc4_key *key)
{
   unsigned char index1;
   unsigned char index2;
   unsigned char* state;
   short counter;     

   state = &key->state[0];         
   for(counter = 0; counter < 256; counter++)              
   state[counter] = counter;               
   key->x = 0;     
   key->y = 0;     
   index1 = 0;     
   index2 = 0;             
   for(counter = 0; counter < 256; counter++)      
   {               
      index2 = (key_data_ptr[index1] + state[counter] +
                index2) % 256;                
      swap_byte(&state[counter], &state[index2]);            
      
      index1 = (index1 + 1) % key_data_len;  
   }       
}

static void rc4(unsigned char *buffer_ptr, int buffer_len, rc4_key *key)
{ 
   unsigned char x;
   unsigned char y;
   unsigned char* state;
   unsigned char xorIndex;
   short counter;              
   
   x = key->x;     
   y = key->y;     
   
   state = &key->state[0];         
   for(counter = 0; counter < buffer_len; counter ++)      
   {               
      x = (x + 1) % 256;                      
      y = (state[x] + y) % 256;               
      swap_byte(&state[x], &state[y]);                        
      
      xorIndex = (state[x] + state[y]) % 256;                 
      
      buffer_ptr[counter] ^= state[xorIndex];         
   }               
   key->x = x;     
   key->y = y;
}

static void swap_byte(unsigned char *a, unsigned char *b)
{
   unsigned char swapByte; 
   
   swapByte = *a; 
   *a = *b;      
   *b = swapByte;
}

/* Dummy _des_crypt function that uses RC4 with a 40 bit key */
int _rc4_crypt(buf, len, desp)
	char *buf;
	int len;
	struct desparams *desp;
{
	struct rc4_key rc4k;

	/*
	 * U.S. government anti-crypto weasels take
	 * note: although we are supplied with a 64 bit
	 * key, we're only passing 40 bits to the RC4
	 * encryption code. So there.
	 */
	prepare_key(desp->des_key, 5, &rc4k);
	rc4(buf, len, &rc4k);

	return(DESERR_NOHWDEVICE);
}

int (*_my_crypt)__P((char *, int, struct desparams *)) = NULL;

static void *dlhandle;

#ifndef _PATH_USRLIB
#define _PATH_USRLIB "/usr/lib"
#endif

#ifndef LIBDES
#define LIBDES "libdes.so.3."
#endif

void load_des(warn, libpath)
	int warn;
	char *libpath;
{
	DIR *dird;
	struct dirent *dirp;
	char dlpath[MAXPATHLEN];
	int minor = -1;
	int len;

	if (libpath == NULL) {
		len = strlen(LIBDES);
		if ((dird = opendir(_PATH_USRLIB)) == NULL)
			err(1, "opendir(/usr/lib) failed");

		while ((dirp = readdir(dird)) != NULL) {
			/* must have a minor number */
			if (strlen(dirp->d_name) <= len)
				continue;
			if (!strncmp(dirp->d_name, LIBDES, len)) {
				if (atoi((dirp->d_name + len + 1)) > minor) {
					minor = atoi((dirp->d_name + len + 1));
					snprintf(dlpath,sizeof(dlpath),"%s/%s",
						_PATH_USRLIB, dirp->d_name);
				}
			}
		}

		closedir(dird);
	} else
		snprintf(dlpath, sizeof(dlpath), "%s", libpath);

	if (dlpath != NULL && (dlhandle = dlopen(dlpath, 0444)) != NULL)
		_my_crypt = (int (*)())dlsym(dlhandle, "__des_crypt");

	if (_my_crypt == NULL) {
		if (dlhandle != NULL)
			dlclose(dlhandle);
		_my_crypt = &_rc4_crypt;
		if (warn) {
			printf ("DES support disabled -- using RC4 instead.\n");
			printf ("Warning: RC4 cipher is not compatible with ");
			printf ("other Secure RPC implementations.\nInstall ");
			printf ("the FreeBSD 'des' distribution to enable");
			printf (" DES encryption.\n");
		}
	} else {
		if (warn) {
			printf ("DES support enabled\n");
			printf ("Using %s shared object.\n", dlpath);
		}
	}

	return;
}

desresp *
des_crypt_1_svc(desargs *argp, struct svc_req *rqstp)
{
	static desresp  result;
	struct desparams dparm;

	if (argp->desbuf.desbuf_len > DES_MAXDATA) {
		result.stat = DESERR_BADPARAM;
		return(&result);
	}

	bcopy(argp->des_key, dparm.des_key, 8);
	bcopy(argp->des_ivec, dparm.des_ivec, 8);
	dparm.des_mode = argp->des_mode;
	dparm.des_dir = argp->des_dir;

#ifdef BROKEN_DES
	dparm.UDES.UDES_buf = argp->desbuf.desbuf_val;
#endif
	result.stat = _my_crypt(argp->desbuf.desbuf_val,
				argp->desbuf.desbuf_len,
				&dparm);

	if (result.stat == DESERR_NONE || result.stat == DESERR_NOHWDEVICE) {
		bcopy(dparm.des_ivec, result.des_ivec, 8);
		result.desbuf.desbuf_len = argp->desbuf.desbuf_len;
		result.desbuf.desbuf_val = argp->desbuf.desbuf_val;
	}

	return (&result);
}
