/*

Functions related to time:

1) rfc (string) time to unix-time
2) unix-time to rfc (string) time
3) current time to rfc (string) time for the "Date:" header

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

#include <linux/time.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/ctype.h>


#include "times.h"
#include "prototypes.h"
static char *dayName[7] = {
	"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

static char *monthName[12] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};


char CurrentTime[64];
int  CurrentTime_i;


static char itoa_h[60]={'0','0','0','0','0','0','0','0','0','0',
		 '1','1','1','1','1','1','1','1','1','1',
		 '2','2','2','2','2','2','2','2','2','2',
		 '3','3','3','3','3','3','3','3','3','3',
		 '4','4','4','4','4','4','4','4','4','4',
		 '5','5','5','5','5','5','5','5','5','5'};
		
static char itoa_l[60]={'0','1','2','3','4','5','6','7','8','9',
		 '0','1','2','3','4','5','6','7','8','9',
		 '0','1','2','3','4','5','6','7','8','9',
		 '0','1','2','3','4','5','6','7','8','9',
		 '0','1','2','3','4','5','6','7','8','9',
		 '0','1','2','3','4','5','6','7','8','9'};
void time_Unix2RFC(const time_t Zulu,char *Buffer)
{
	int Y=0,M=0,D=0;
	int H=0,Min=0,S=0,WD=0;
	int I,I2;
	time_t rest;
	
	
		
	I=0;
	while (I<KHTTPD_NUMYEARS)
	{
		if (TimeDays[I][0]>Zulu) 
		   break;
		I++;
	}
	
	Y=--I;
	if (I<0) 
	{
		Y=0;
		goto BuildYear;
	}
	I2=0;
	while (I2<=12)
	{
		if (TimeDays[I][I2]>Zulu) 
		   break;
		I2++;
	}			   
	
	M=I2-1;
	
	rest=Zulu - TimeDays[Y][M];
	WD=WeekDays[Y][M];
	D=rest/86400;
	rest=rest%86400;
	WD+=D;
	WD=WD%7;
	H=rest/3600;
	rest=rest%3600;
	Min=rest/60;
	rest=rest%60;
	S=rest;
	
BuildYear:
	Y+=KHTTPD_YEAROFFSET;
	
	
	/* Format:  Day, 01 Mon 1999 01:01:01 GMT */
	
/*	
	We want to do 
	
	sprintf( Buffer, "%s, %02i %s %04i %02i:%02i:%02i GMT",
		dayName[ WD ], D+1, monthName[ M ], Y,
		H, Min, S
	);
	
	but this is very expensive. Since the string is fixed length,
	it is filled manually.
*/
	Buffer[0]=dayName[WD][0];
	Buffer[1]=dayName[WD][1];
	Buffer[2]=dayName[WD][2];
	Buffer[3]=',';
	Buffer[4]=' ';
	Buffer[5]=itoa_h[D+1];
	Buffer[6]=itoa_l[D+1];
	Buffer[7]=' ';
	Buffer[8]=monthName[M][0];
	Buffer[9]=monthName[M][1];
	Buffer[10]=monthName[M][2];
	Buffer[11]=' ';
	Buffer[12]=itoa_l[Y/1000];
	Buffer[13]=itoa_l[(Y/100)%10];
	Buffer[14]=itoa_l[(Y/10)%10];
	Buffer[15]=itoa_l[Y%10];
	Buffer[16]=' ';
	Buffer[17]=itoa_h[H];
	Buffer[18]=itoa_l[H];
	Buffer[19]=':';
	Buffer[20]=itoa_h[Min];
	Buffer[21]=itoa_l[Min];
	Buffer[22]=':';
	Buffer[23]=itoa_h[S];
	Buffer[24]=itoa_l[S];
	Buffer[25]=' ';
	Buffer[26]='G';
	Buffer[27]='M';
	Buffer[28]='T';
	Buffer[29]=0;
	
	
		
	
}

void UpdateCurrentDate(void)
{
   struct timeval tv;
   
   do_gettimeofday(&tv);
   if (CurrentTime_i!=tv.tv_sec)
	   time_Unix2RFC(tv.tv_sec,CurrentTime);
   
   CurrentTime_i = tv.tv_sec;
}

static int MonthHash[32] = {0,0,7,0,0,0,0,0,0,0,0,3,0,0,0,2,6,0,5,0,9,8,4,0,0,11,1,10,0,0,0,0};

#define is_digit(c)	((c) >= '0' && (c) <= '9')

__inline static int skip_atoi(char **s)
{
	int i=0;

	while (is_digit(**s))
		i = i*10 + *((*s)++) - '0';
	return i;
}

time_t mimeTime_to_UnixTime(char *Q)
{
	int Y,M,D,H,Min,S;
	unsigned int Hash;
	time_t Temp;
	char *s,**s2;
	
	s=Q;
	s2=&s;
	
	if (strlen(s)<30) return 0;
	if (s[3]!=',') return 0;
	if (s[19]!=':') return 0;
	
	s+=5; /* Skip day of week */
	D = skip_atoi(s2);  /*  Day of month */
	s++;
	Hash = (unsigned char)s[0]+(unsigned char)s[2];
	Hash = (Hash<<1) + (unsigned char)s[1];
	Hash = (Hash&63)>>1;
	M = MonthHash[Hash];
	s+=4;
	Y = skip_atoi(s2); /* Year */
	s++;
	H = skip_atoi(s2); /* Hour */
	s++;
	Min = skip_atoi(s2); /* Minutes */
	s++;
	S = skip_atoi(s2); /* Seconds */
	s++;
	if ((s[0]!='G')||(s[1]!='M')||(s[2]!='T')) 
	{	
  		return 0; /* No GMT */
  	}

	if (Y<KHTTPD_YEAROFFSET) Y = KHTTPD_YEAROFFSET;
	if (Y>KHTTPD_YEAROFFSET+9) Y = KHTTPD_YEAROFFSET+9;
	
	Temp = 	TimeDays[Y-KHTTPD_YEAROFFSET][M];
	Temp += D*86400+H*3600+Min*60+S;
	
	return Temp;  
}
