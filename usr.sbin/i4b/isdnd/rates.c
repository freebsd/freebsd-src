/*
 *   Copyright (c) 1997 Gary Jennejohn. All rights reserved.
 * 
 *   Copyright (c) 1997, 1999 Hellmuth Michaelis. All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *   3. Neither the name of the author nor the names of any co-contributors
 *      may be used to endorse or promote products derived from this software
 *      without specific prior written permission.
 *   4. Altered versions must be plainly marked as such, and must not be
 *      misrepresented as being the original software and/or documentation.
 *   
 *   THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 *   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *   ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 *   FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 *   OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *   HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 *   OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 *   SUCH DAMAGE.
 *
 *---------------------------------------------------------------------------
 *
 *	i4b daemon - charging rates description file handling
 *	-----------------------------------------------------
 *
 *	$Id: rates.c,v 1.11 2000/10/09 12:53:29 hm Exp $ 
 *
 * $FreeBSD$
 *
 *      last edit-date: [Mon Dec 13 21:48:31 1999]
 *
 *---------------------------------------------------------------------------*/

static char error[256];

static int getrate(int rate_type);

#ifdef PARSE_DEBUG_MAIN

#include <stdio.h>

#define MAIN

#define ERROR (-1)

extern int got_rate;

int main( int argc, char **argv )
{
	int ret;
	ret = readrates("/etc/isdn/isdnd.rates");
	if(ret == ERROR)
		fprintf(stderr, "readrates returns [%d], [%s]\n", ret, error);
	else
		{
		int type = 0;

		got_rate = 1;

		fprintf(stderr, "readrates returns [%d]\n", ret);

		for( type=0; type<4; type++ )
			{
			int unit = getrate( type );
			fprintf(stderr, "getrate(%d) => %d\n", type, unit );
			}
		}

	return(ret);
}

#endif

#include "isdnd.h"

/*---------------------------------------------------------------------------*
 *	parse rates file
 *---------------------------------------------------------------------------*/
int
readrates(char *filename)
{
	char buffer[MAXPATHLEN];
	register char *bp;
	struct rates *rt, *ort;
	int rateindx;
	int indx;
	int line = 0;
	FILE *fp;
	int first;
#if DEBUG
	int i, j;
#endif
	
	indx = 0;
	rt = ort = NULL;

	if((fp = fopen(filename, "r")) == NULL)
	{
		snprintf(error, sizeof(error), "error open %s: %s", filename, sys_errlist[errno]);
		rate_error = error;
		return(WARNING);
	}

	while((fgets(buffer, MAXPATHLEN, fp)) != NULL)
	{
		line++;

/* comments */
		if(buffer[0] == '#'  || buffer[0] == ' ' ||
		   buffer[0] == '\t' || buffer[0] == '\n')
		{
			continue;
		}

		bp = &buffer[0];

		/* rate type */

		if (*bp == 'r' && *(bp+1) == 'a' && isdigit(*(bp+2)))
		{
				rateindx = *(bp+2) - '0';
				bp += 3;

				/* eat space delimiter */

				while(isspace(*bp))
					bp++;
		}
		else
		{
			snprintf(error, sizeof(error), "rates: invalid rate type %c%c%c in line %d", *bp, *(bp+1), *(bp+2), line);
			goto rate_error;
		}
		if (rateindx >= NRATES)
		{
			snprintf(error, sizeof(error), "rates: invalid rate index %d in line %d", rateindx, line);
			goto rate_error;
		}

		/* day */
		
		if(isdigit(*bp) && *bp >= '0' && *bp <= '6')
		{
			indx = *bp - '0';

			DBGL(DL_RATES, (log(LL_DBG, "rates: index = %d", indx)));
		}
		else
		{
			snprintf(error, sizeof(error), "rates: invalid day digit %c in line %d", *bp, line);
			goto rate_error;
		}

		if(rates[rateindx][indx] == NULL)
		{
			rt = (struct rates *)malloc(sizeof (struct rates));
			if (rt == NULL)
			{
				snprintf(error, sizeof(error), "rates: cannot malloc space for rate structure");
				goto rate_error;
		  	}
			rt->next = NULL;
		  	rates[rateindx][indx] = rt;
		}

		bp++;
		
		/* eat space delimiter */

		while(isspace(*bp))
			bp++;

		/* now loop to get the rates entries */

		first = 1;
		
		while(*bp && isdigit(*bp))
		{
			int hour = 0;
			int min = 0;

			if(first)
			{
				first = 0;
			}
			else
			{
				ort = rt;
	
				rt = (struct rates *)malloc(sizeof (struct rates));
				if (rt == NULL)
				{
					snprintf(error, sizeof(error), "rates: cannot malloc space2 for rate structure");
					goto rate_error;
			  	}
				ort->next = rt;
				rt->next = NULL;
			}
			
			/* start hour */
			
			if(isdigit(*bp) && isdigit(*(bp+1)))
			{
				hour = atoi(bp);
				bp += 2;
			}
		  	else
			{
				snprintf(error, sizeof(error), "rates: start_hr error in line %d", line);
				goto rate_error;
		  	}

			/* point */
			
		  	if(*bp == '.')
		  	{
		  		bp++;
		  	}
		  	else
		  	{
				snprintf(error, sizeof(error), "rates: no '.' after start_hr in line %d", line);
				goto rate_error;
			}
		  	
			/* start minute */
			
			if(isdigit(*bp) && isdigit(*(bp+1)))
			{
				min = atoi(bp);
				bp += 2;
			}
		  	else
			{
				snprintf(error, sizeof(error), "rates: start_min error in line %d", line);
				goto rate_error;
		  	}

			rt->start_time = hour*60 + min;

			/* minus */
			
		  	if(*bp == '-')
		  	{
		  		bp++;
		  	}
		  	else
		  	{
				snprintf(error, sizeof(error), "rates: no '-' after start_min in line %d", line);
				goto rate_error;
			}

			/* end hour */
			
			if(isdigit(*bp) && isdigit(*(bp+1)))
			{
				hour = atoi(bp);
				bp += 2;
			}
		  	else
			{
				snprintf(error, sizeof(error), "rates: end_hr error in line %d", line);
				goto rate_error;
		  	}

			/* point */
			
		  	if(*bp == '.')
		  	{
		  		bp++;
		  	}
		  	else
		  	{
				snprintf(error, sizeof(error), "rates: no '.' after end_hr in line %d", line);
				goto rate_error;
			}
		  	
			/* end minute */
			
			if(isdigit(*bp) && isdigit(*(bp+1)))
			{
				min = atoi(bp);
				bp += 2;
			}
		  	else
			{
				snprintf(error, sizeof(error), "rates: end_min error in line %d", line);
				goto rate_error;
		  	}

			/* if hour is 0 assume it means midnight */
			if( hour == 0 )
				hour = 24;
			rt->end_time = hour * 60 + min;

			if( rt->end_time <= rt->start_time )
				{
				snprintf(error, sizeof(error), "rates: end_time must be greater then start_time %d", line);
				goto rate_error;
				}

			/* colon */
			
		  	if(*bp == ':')
		  	{
		  		bp++;
		  	}
		  	else
		  	{
				snprintf(error, sizeof(error), "rates: no ':' after end_min in line %d", line);
				goto rate_error;
			}

			/* time */
			
			if(isdigit(*bp))
			{
				rt->rate = atoi(bp);
				while(!isspace(*bp))
					bp++;
			}
		  	else
			{
				snprintf(error, sizeof(error), "rates: first rate digit error in line %d", line);
				goto rate_error;
		  	}

			/* eat space delimiter */

			while(isspace(*bp))
				bp++;
		}
	}

#if DEBUG
	if(debug_flags & DL_RATES)
	{
		for (j = 0; j < NRATES; j++)
		{
			for (i = 0; i < NDAYS; i++)
			{
				if (rates [j][i] != NULL)
				{
					rt = rates [j][i];
					for (; rt; rt = rt->next)
					{
						log(LL_DBG, "rates: index %d day %d = %d.%2.2d-%d.%2.2d:%d",
							j, i, rt->start_time/60, rt->start_time%60,
							rt->end_time/60,rt->end_time%60,rt->rate);
					}
				}
				else
				{
					log(LL_DBG, "rates: NO entry for day %d !!\n", i);
				}
			}
		}
	}
#endif
	fclose(fp);
	return(GOOD);

rate_error:
	fclose(fp);
	rate_error = error;
	return(ERROR);
}

#ifndef PARSE_DEBUG_MAIN

/*---------------------------------------------------------------------------*
 *	get unit length time from configured source
 *---------------------------------------------------------------------------*/
int
get_current_rate(cfg_entry_t *cep, int logit)
{
	int rt;
	
	switch(cep->unitlengthsrc)
	{
		case ULSRC_CMDL:	/* specified on commandline     */
			if(logit)
				log(LL_CHD, "%05d %s rate %d sec/unit (cmdl)",
					cep->cdid, cep->name, unit_length);
			return(unit_length);
			break;

		case ULSRC_CONF:	/* get it from config file      */
			if(logit)
				log(LL_CHD, "%05d %s rate %d sec/unit (conf)",
					cep->cdid, cep->name, cep->unitlength);
			return(cep->unitlength);

		case ULSRC_RATE:	/* get it dynamic from ratesfile*/
			if(!got_rate)	/* got valid rates struct ?? */
			{
				if(logit)
					log(LL_CHD, "%05d %s rate %d sec/unit (no ratefile)",
						cep->cdid, cep->name, UNITLENGTH_DEFAULT);
				return(UNITLENGTH_DEFAULT);
			}
			if((cep->ratetype >= NRATES) ||
			   (cep->ratetype == INVALID_RATE))
			{
				if(logit)
					log(LL_CHD, "%05d %s rate %d sec/unit (rate out of range)",
						cep->cdid, cep->name, UNITLENGTH_DEFAULT);
				return(UNITLENGTH_DEFAULT);
			}
			
			if((rt = getrate(cep->ratetype)) != -1)
			{
				if(logit)
					log(LL_CHD, "%05d %s rate %d sec/unit (rate)",
						cep->cdid, cep->name, rt);
				return(rt);
			}

			if(logit)			
				log(LL_CHD, "%05d %s rate %d sec/unit (ratescan fail)",
					cep->cdid, cep->name, UNITLENGTH_DEFAULT);

			return(UNITLENGTH_DEFAULT);
			break;

		case ULSRC_DYN:	/* dynamically calculated from AOC */
			if((rt = getrate(cep->ratetype)) != -1)
			{
				if(logit)
					log(LL_CHD, "%05d %s rate %d sec/unit (aocd, rate)",
						cep->cdid, cep->name, rt);
				return(rt);
			}
			if(logit)
				log(LL_CHD, "%05d %s rate %d sec/unit (aocd, default)",
					cep->cdid, cep->name, UNITLENGTH_DEFAULT);

			return(UNITLENGTH_DEFAULT);
			break;

		default:
			if(logit)
				log(LL_CHD, "%05d %s rate %d sec/unit (unitlen unknown)",
					cep->cdid, cep->name, UNITLENGTH_DEFAULT);

			return(UNITLENGTH_DEFAULT);
			break;
	}
}
#endif /* PARSE_DEBUG_MAIN */


/*---------------------------------------------------------------------------*
 *	get the currently active rate
 *---------------------------------------------------------------------------*/
static int
getrate(int rate_type )
{
	struct tm *ptr;
	time_t now;
	register struct rates *hd;
	int time_now;

	if((!got_rate) ||
	   (rate_type >= NRATES) ||
	   (rate_type == INVALID_RATE))
	{
		return -1;
	}

	time(&now);			/* get current time */

	ptr = localtime(&now);

	time_now = ptr->tm_hour*60 + ptr->tm_min;

	/* walk thru the rates for weekday until rate for current time found */

	for (hd = rates[rate_type][ptr->tm_wday]; hd; hd = hd->next)
	{
		/* current time within window ? */
		if((time_now >= hd->start_time ) &&
		   (time_now < hd->end_time ))
		{
			DBGL(DL_RATES, (log(LL_DBG, "rate=%d sec/unit (day=%d, beg=%d:%2.2d, end=%d:2.2d, current=%d:%2.2d)",
				hd->rate,
				ptr->tm_wday,
				hd->start_time/60, hd->start_time%60,
				hd->end_time/60, hd->end_time%60,
				time_now/60, time_now%60)));
				
			return hd->rate;
		}
	}
	return -1;
}

/* EOF */
