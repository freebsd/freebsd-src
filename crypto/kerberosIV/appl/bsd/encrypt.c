/* Copyright (C) 1995 Eric Young (eay@mincom.oz.au)
 * All rights reserved.
 * 
 * This file is part of an SSL implementation written
 * by Eric Young (eay@mincom.oz.au).
 * The implementation was written so as to conform with Netscapes SSL
 * specification.  This library and applications are
 * FREE FOR COMMERCIAL AND NON-COMMERCIAL USE
 * as long as the following conditions are aheared to.
 * 
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.  If this code is used in a product,
 * Eric Young should be given attribution as the author of the parts used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by Eric Young (eay@mincom.oz.au)
 * 
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */

#include "bsd_locl.h"

RCSID("$Id: encrypt.c,v 1.4 1999/06/17 18:47:26 assar Exp $");

/* replacements for htonl and ntohl since I have no idea what to do
 * when faced with machines with 8 byte longs. */
#define HDRSIZE 4

#define n2l(c,l)	(l =((u_int32_t)(*((c)++)))<<24, \
			 l|=((u_int32_t)(*((c)++)))<<16, \
			 l|=((u_int32_t)(*((c)++)))<< 8, \
			 l|=((u_int32_t)(*((c)++))))

#define l2n(l,c)	(*((c)++)=(unsigned char)(((l)>>24)&0xff), \
			 *((c)++)=(unsigned char)(((l)>>16)&0xff), \
			 *((c)++)=(unsigned char)(((l)>> 8)&0xff), \
			 *((c)++)=(unsigned char)(((l)    )&0xff))

/* This has some uglies in it but it works - even over sockets. */
extern int errno;
int des_rw_mode=DES_PCBC_MODE;
int LEFT_JUSTIFIED = 0;

int
des_enc_read(int fd, char *buf, int len, struct des_ks_struct *sched, des_cblock *iv)
{
  /* data to be unencrypted */
  int net_num=0;
  unsigned char net[DES_RW_BSIZE];
  /* extra unencrypted data 
   * for when a block of 100 comes in but is des_read one byte at
   * a time. */
  static char unnet[DES_RW_BSIZE];
  static int unnet_start=0;
  static int unnet_left=0;
  int i;
  long num=0,rnum;
  unsigned char *p;

  /* left over data from last decrypt */
  if (unnet_left != 0)
    {
      if (unnet_left < len)
	{
	  /* we still still need more data but will return
	   * with the number of bytes we have - should always
	   * check the return value */
	  memcpy(buf,&(unnet[unnet_start]),unnet_left);
	  /* eay 26/08/92 I had the next 2 lines
	   * reversed :-( */
	  i=unnet_left;
	  unnet_start=unnet_left=0;
	}
      else
	{
	  memcpy(buf,&(unnet[unnet_start]),len);
	  unnet_start+=len;
	  unnet_left-=len;
	  i=len;
	}
      return(i);
    }

  /* We need to get more data. */
  if (len > DES_RW_MAXWRITE) len=DES_RW_MAXWRITE;

  /* first - get the length */
  net_num=0;
  while (net_num < HDRSIZE) 
    {
      i=read(fd,&(net[net_num]),(unsigned int)HDRSIZE-net_num);
      if ((i == -1) && (errno == EINTR)) continue;
      if (i <= 0) return(0);
      net_num+=i;
    }

  /* we now have at net_num bytes in net */
  p=net;
  num=0;
  n2l(p,num);
  /* num should be rounded up to the next group of eight
   * we make sure that we have read a multiple of 8 bytes from the net.
   */
  if ((num > DES_RW_MAXWRITE) || (num < 0)) /* error */
    return(-1);
  rnum=(num < 8)?8:((num+7)/8*8);

  net_num=0;
  while (net_num < rnum)
    {
      i=read(fd,&(net[net_num]),(unsigned int)rnum-net_num);
      if ((i == -1) && (errno == EINTR)) continue;
      if (i <= 0) return(0);
      net_num+=i;
    }

  /* Check if there will be data left over. */
  if (len < num)
    {
      if (des_rw_mode & DES_PCBC_MODE)
	des_pcbc_encrypt((des_cblock *)net,(des_cblock *)unnet,
		     num,sched,iv,DES_DECRYPT);
      else
	des_cbc_encrypt((des_cblock *)net,(des_cblock *)unnet,
		    num,sched,iv,DES_DECRYPT);
      memcpy(buf,unnet,len);
      unnet_start=len;
      unnet_left=num-len;

      /* The following line is done because we return num
       * as the number of bytes read. */
      num=len;
    }
  else
    {
      /* >output is a multiple of 8 byes, if len < rnum
       * >we must be careful.  The user must be aware that this
       * >routine will write more bytes than he asked for.
       * >The length of the buffer must be correct.
       * FIXED - Should be ok now 18-9-90 - eay */
      if (len < rnum)
	{
	  char tmpbuf[DES_RW_BSIZE];

	  if (des_rw_mode & DES_PCBC_MODE)
	    des_pcbc_encrypt((des_cblock *)net,
			 (des_cblock *)tmpbuf,
			 num,sched,iv,DES_DECRYPT);
	  else
	    des_cbc_encrypt((des_cblock *)net,
			(des_cblock *)tmpbuf,
			num,sched,iv,DES_DECRYPT);

	  /* eay 26/08/92 fix a bug that returned more
	   * bytes than you asked for (returned len bytes :-( */
	  if (LEFT_JUSTIFIED || (len >= 8))
	      memcpy(buf,tmpbuf,num);
	  else
	      memcpy(buf,tmpbuf+(8-num),num); /* Right justified */
	}
      else if (num >= 8)
	{
	  if (des_rw_mode & DES_PCBC_MODE)
	    des_pcbc_encrypt((des_cblock *)net,
			 (des_cblock *)buf,num,sched,iv,
			 DES_DECRYPT);
	  else
	    des_cbc_encrypt((des_cblock *)net,
			(des_cblock *)buf,num,sched,iv,
			DES_DECRYPT);
	}
      else
	{
	  if (des_rw_mode & DES_PCBC_MODE)
	    des_pcbc_encrypt((des_cblock *)net,
			 (des_cblock *)buf,8,sched,iv,
			 DES_DECRYPT);
	  else
	    des_cbc_encrypt((des_cblock *)net,
			(des_cblock *)buf,8,sched,iv,
			DES_DECRYPT);
	  if (!LEFT_JUSTIFIED)
	      memcpy(buf, buf+(8-num), num); /* Right justified */
	}
    }
  return(num);
}

int
des_enc_write(int fd, char *buf, int len, struct des_ks_struct *sched, des_cblock *iv)
{
  long rnum;
  int i,j,k,outnum;
  char outbuf[DES_RW_BSIZE+HDRSIZE];
  char shortbuf[8];
  char *p;
  static int start=1;

  /* If we are sending less than 8 bytes, the same char will look
   * the same if we don't pad it out with random bytes */
  if (start)
    {
      start=0;
      srand(time(NULL));
    }

  /* lets recurse if we want to send the data in small chunks */
  if (len > DES_RW_MAXWRITE)
    {
      j=0;
      for (i=0; i<len; i+=k)
	{
	  k=des_enc_write(fd,&(buf[i]),
			  ((len-i) > DES_RW_MAXWRITE)?DES_RW_MAXWRITE:(len-i),sched,iv);
	  if (k < 0)
	    return(k);
	  else
	    j+=k;
	}
      return(j);
    }

  /* write length first */
  p=outbuf;
  l2n(len,p);

  /* pad short strings */
  if (len < 8)
    {
	if (LEFT_JUSTIFIED)
	    {
		p=shortbuf;
		memcpy(shortbuf,buf,(unsigned int)len);
		for (i=len; i<8; i++)
		    shortbuf[i]=rand();
		rnum=8;
	    }
	else
	    {
		p=shortbuf;
		for (i=0; i<8-len; i++)
		    shortbuf[i]=rand();
		memcpy(shortbuf + 8 - len, buf, len);
		rnum=8;
	    }
    }
  else
    {
      p=buf;
      rnum=((len+7)/8*8);	/* round up to nearest eight */
    }

  if (des_rw_mode & DES_PCBC_MODE)
    des_pcbc_encrypt((des_cblock *)p,(des_cblock *)&(outbuf[HDRSIZE]),
		 (long)((len<8)?8:len),sched,iv,DES_ENCRYPT); 
  else
    des_cbc_encrypt((des_cblock *)p,(des_cblock *)&(outbuf[HDRSIZE]),
		(long)((len<8)?8:len),sched,iv,DES_ENCRYPT); 

  /* output */
  outnum=rnum+HDRSIZE;

  for (j=0; j<outnum; j+=i)
    {
      /* eay 26/08/92 I was not doing writing from where we
       * got upto. */
      i=write(fd,&(outbuf[j]),(unsigned int)(outnum-j));
      if (i == -1)
	{
	  if (errno == EINTR)
	    i=0;
	  else			/* This is really a bad error - very bad
				 * It will stuff-up both ends. */
	    return(-1);
	}
    }

  return(len);
}
