/*

This program generates the "times.h" file with the zulu-times of the first of
every month of a decade.

*/
/****************************************************************
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2, or (at your option)
 *	any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 ****************************************************************/

#include <time.h>
#include <stdio.h>

static time_t GetDay(int D,int M,int Y)
{
	struct tm TM;
	
	TM.tm_sec    	= 0;
	TM.tm_min	= 0;
	TM.tm_hour	= 0;
	TM.tm_mday	= D;
	TM.tm_mon	= M;
	TM.tm_wday	= 0;
	TM.tm_yday	= 0;
	TM.tm_year	= Y-1900;
	TM.tm_isdst	= 0;
	
	return mktime(&TM);
	
}
static int WeekGetDay(int D,int M,int Y)
{
	struct tm TM;
	
	TM.tm_sec    	= 0;
	TM.tm_min	= 0;
	TM.tm_hour	= 0;
	TM.tm_mday	= D;
	TM.tm_mon	= M;
	TM.tm_year	= Y-1900;
	TM.tm_isdst	= 0;
	TM.tm_wday	= 0;
	TM.tm_yday	= 0;
	
	(void)mktime(&TM);
	
	return TM.tm_wday;
	
}

int main(void)
{
	int M,Y;
	FILE *file;
	
	file=fopen("times.h","w");
	
	if (file==NULL) 
		return 0; 
	
	fprintf(file,"static time_t TimeDays[10][13] = { \n");
	
	Y=1997;
	while (Y<2007)
	{
		M=0;
		fprintf(file," { ");
		while (M<12)
		{
			fprintf(file,"%i",(int)GetDay(1,M,Y));
     		  	fprintf(file,",\t");
		
			M++;
		}
		
		fprintf(file,"%i } ",(int)GetDay(1,0,Y+1));
		if (Y!=2006) fprintf(file,",");
		fprintf(file,"\n");
		Y++;
	}
	fprintf(file,"};\n");

	fprintf(file,"static int WeekDays[10][13] = { \n");
	
	Y=1997;
	while (Y<2007)
	{
		M=0;
		fprintf(file," { ");
		while (M<12)
		{
			fprintf(file,"%i",(int)WeekGetDay(1,M,Y));
     		  	fprintf(file,",\t");
		
			M++;
		}
		
		fprintf(file,"%i } ",(int)WeekGetDay(1,0,Y+1));
		if (Y!=2006) fprintf(file,",");
		fprintf(file,"\n");
		Y++;
	}
	fprintf(file,"};\n");
	fprintf(file,"#define KHTTPD_YEAROFFSET   1997\n");
	fprintf(file,"#define KHTTPD_NUMYEARS     10\n");
	(void)fclose(file);
	
	return 0;
}
