/*
 * Copyright (c) 2000, 2001 Hellmuth Michaelis. All rights reserved.
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
 *	isdnd - holiday file handling
 *      =============================
 *
 * $FreeBSD$
 *
 *      last edit-date: [Wed May  2 09:42:56 2001]
 *
 *	Format:
 *
 *	day.month.year	optional comment (different day every year)	or
 *	day.month	optional comment (same day every year)
 *
 *	i.e.:
 *
 *	23.4.2000	Ostersonntag
 *	3.10		Tag der deutschen Einheit
 *
 *----------------------------------------------------------------------------*/

#include "isdnd.h"

struct holiday {
	int	day;
	int	month;
	int	year;
	struct holiday *next;
};

static struct holiday *firsth = NULL;

#define MAXBUFSZ	256

static void free_holiday(struct holiday *ptr);

/*---------------------------------------------------------------------------*
 *	read in and init holidayes
 *---------------------------------------------------------------------------*/
void 
init_holidays(char *filename)
{
	FILE *fp;
	unsigned char buffer[MAXBUFSZ + 1];
	struct holiday *newh = NULL;
	struct holiday *lasth = NULL;
	int ret;
	int day, month, year;
	
	firsth = NULL;
	
	if((fp = fopen(filename, "r")) == NULL)
	{
		DBGL(DL_VALID, (log(LL_DBG, "init_holiday: error opening holidayfile %s: %s!", filename, strerror(errno))));
		return;
	}

	while((fgets(buffer, MAXBUFSZ, fp)) != NULL)
	{
		if(buffer[0] == '#'  || buffer[0] == ' ' ||
		   buffer[0] == '\t' || buffer[0] == '\n')
		{
			continue;
		}

		ret = sscanf(buffer, "%d.%d.%d", &day, &month, &year);

		if(ret != 3)
		{
			ret = sscanf(buffer, "%d.%d", &day, &month);
			if(ret != 2)
			{
				log(LL_ERR, "init_holiday: parse error for string [%s]!", buffer);
				exit(1);
			}
			year = 0;
		}

		if((newh = (struct holiday *) malloc(sizeof(struct holiday))) == NULL)
		{
			log(LL_ERR, "init_holiday: malloc failed for struct holiday!\n");
			exit(1);
		}

		if(year)
		{
			DBGL(DL_VALID, (log(LL_DBG, "init_holidays: add %d.%d.%d", day, month, year)));
		}
		else
		{
			DBGL(DL_VALID, (log(LL_DBG, "init_holidays: add %d.%d", day, month)));
		}
		
		newh->day = day;
		newh->month = month;
		newh->year = year;
		newh->next = NULL;
		
		if(firsth == NULL)
		{
			firsth = newh;
		}
		else
		{
			lasth->next = newh;
		}
		lasth = newh;			
	}
	fclose(fp);
}

/*---------------------------------------------------------------------------*
 *	free all holidays
 *---------------------------------------------------------------------------*/
void
free_holidays(void)
{
	free_holiday(firsth);
}

/*---------------------------------------------------------------------------*
 *	free holidayes
 *---------------------------------------------------------------------------*/
static void
free_holiday(struct holiday *ptr)
{

	if(ptr == NULL)
		return;

	if(ptr->next != NULL)
		free_holiday(ptr->next);

	free(ptr);
}
	
/*---------------------------------------------------------------------------*
 *	check if date/month/year is a holiday
 *---------------------------------------------------------------------------*/
int
isholiday(int d, int m, int y)
{
	struct holiday *ch = NULL;

	if(firsth == NULL)
		return(0);

	ch = firsth;

	for(;;)
	{
		if(ch->day == d && ch->month == m)
		{
			if(ch->year == 0)
			{
				DBGL(DL_VALID, (log(LL_DBG, "isholiday: %d.%d is a holiday!", d, m)));
				return(1);
			}
			else if(ch->year == y)
			{
				DBGL(DL_VALID, (log(LL_DBG, "isholiday: %d.%d.%d is a holiday!", d, m, y)));
				return(1);
			}
		}

		if(ch->next == NULL)
			break;

		ch = ch->next;
	}
	return(0);
}
			
/* EOF */
