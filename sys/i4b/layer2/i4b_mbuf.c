/*
 * Copyright (c) 1997, 2002 Hellmuth Michaelis. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
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
 *---------------------------------------------------------------------------
 *
 *	i4b - mbuf handling support routines
 *	------------------------------------
 *
 * $FreeBSD$
 *
 *      last edit-date: [Sat Mar  9 17:51:22 2002]
 *
 *---------------------------------------------------------------------------*/

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <net/if.h>

#include <i4b/include/i4b_mbuf.h>

#define I4B_MBUF_DEBUG
#undef I4B_MBUF_TYPE_DEBUG

#ifdef I4B_MBUF_TYPE_DEBUG

#define MT_DCHAN	42
#define MT_BCHAN	43

#define MT_I4B_D	MT_DCHAN
#define MT_I4B_B	MT_BCHAN

#else /* ! I4B_MBUF_TYPE_DEBUG */

#define MT_I4B_D	MT_DATA
#define MT_I4B_B	MT_DATA

#endif /* I4B_MBUF_TYPE_DEBUG */

/*---------------------------------------------------------------------------*
 *	allocate D-channel mbuf space
 *---------------------------------------------------------------------------*/
struct mbuf*
i4b_Dgetmbuf(int len)
{
	struct mbuf *m;

	if(len > MCLBYTES)	/* if length > max extension size */
	{

#ifdef I4B_MBUF_DEBUG
		printf("i4b_getmbuf: error - len(%d) > MCLBYTES(%d)\n",
					len, MCLBYTES);
#endif
		
		return(NULL);
	}

	MGETHDR(m, M_NOWAIT, MT_I4B_D);	/* get mbuf with pkthdr */

	/* did we actually get the mbuf ? */

	if(!m)	
	{

#ifdef I4B_MBUF_DEBUG
		printf("i4b_getbuf: error - MGETHDR failed!\n");
#endif

		return(NULL);
	}

	if(len >= MHLEN)
	{
		MCLGET(m, M_NOWAIT);

		if(!(m->m_flags & M_EXT))
		{
			m_freem(m);

#ifdef I4B_MBUF_DEBUG
			printf("i4b_getbuf: error - MCLGET failed, len(%d)\n", len);
#endif
			
			return (NULL);
		}
	}

	m->m_len = len;

	return(m);
}

/*---------------------------------------------------------------------------*
 *	free a D-channel mbuf
 *---------------------------------------------------------------------------*/
void
i4b_Dfreembuf(struct mbuf *m)
{
	if(m)
		m_freem(m);
}

/*---------------------------------------------------------------------------*
 *	clear a D-channel ifqueue from data
 *---------------------------------------------------------------------------*/
void
i4b_Dcleanifq(struct ifqueue *ifq)
{
	int x = splimp();

	IF_DRAIN(ifq);

	splx(x);
}

/*---------------------------------------------------------------------------*
 *	allocate B-channel mbuf space
 *---------------------------------------------------------------------------*/
struct mbuf*
i4b_Bgetmbuf(int len)
{
	struct mbuf *m;

	if(len > MCLBYTES)	/* if length > max extension size */
	{

#ifdef I4B_MBUF_DEBUG
		printf("i4b_getmbuf: error - len(%d) > MCLBYTES(%d)\n",
					len, MCLBYTES);
#endif
		
		return(NULL);
	}

	MGETHDR(m, M_NOWAIT, MT_I4B_B);	/* get mbuf with pkthdr */

	/* did we actually get the mbuf ? */

	if(!m)	
	{

#ifdef I4B_MBUF_DEBUG
		printf("i4b_getbuf: error - MGETHDR failed!\n");
#endif

		return(NULL);
	}

	if(len >= MHLEN)
	{
		MCLGET(m, M_NOWAIT);

		if(!(m->m_flags & M_EXT))
		{
			m_freem(m);

#ifdef I4B_MBUF_DEBUG
			printf("i4b_getbuf: error - MCLGET failed, len(%d)\n", len);
#endif
			
			return (NULL);
		}
	}

	m->m_len = len;

	return(m);
}

/*---------------------------------------------------------------------------*
 *	free a B-channel mbuf
 *---------------------------------------------------------------------------*/
void
i4b_Bfreembuf(struct mbuf *m)
{
	if(m)
		m_freem(m);
}

/*---------------------------------------------------------------------------*
 *	clear a B-channel ifqueue from data
 *---------------------------------------------------------------------------*/
void
i4b_Bcleanifq(struct ifqueue *ifq)
{
	int x = splimp();
	
	IF_DRAIN(ifq);

	splx(x);
}

/* EOF */
