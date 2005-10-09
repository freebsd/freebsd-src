// $FreeBSD$
// changed ".g711a" to ".al" (-hm)
// Tue Mar  3 02:42:14 MET 1998 	dave@turbocat.de
// started

#define BLK_SIZE	2048
#define SOX		"/usr/local/bin/sox"
#define	ALAWULAW	"/usr/local/bin/alaw2ulaw"

#include <stdio.h>
#include <time.h>

	FILE	*device;
	FILE	*logfile;
	char	srcNum[30];
	char	destNum[30];
	char argbuf[255];
	char tmpBuf[1024] = "";


void writeToPhone (char *path)
{
	char 	buf[BLK_SIZE];
	FILE	*srcfile;
	int		i = 0;
	int		readcount = 0;

	srcfile = fopen(path,"r");
	if (srcfile) {
		for (i=0;i<BLK_SIZE;i++) {
			buf[i] = '\0';
		}
		readcount = BLK_SIZE;
		i = 0;
		do {
     		readcount = fread(buf,1, BLK_SIZE, srcfile);
     		fwrite(buf, 1, readcount, device);
			i = readcount + i;
//			fprintf(logfile,"%d read (%d)\n",i,readcount);
		} while (readcount == BLK_SIZE);
	
		fclose(srcfile);
	} else {
		fprintf(logfile,"Can't open file '%s'\n",path);
	}
}

void readFromPhone (char *path)
{
	char 	buf[BLK_SIZE];
	FILE	*destfile;
	int		i = 0;
	int		readcount = 0;

	destfile = fopen(path,"a");
	if (destfile) {
		for (i=0;i<BLK_SIZE;i++) {
			buf[i] = '\0';
		}
		readcount = BLK_SIZE;
		i = 0;
		do {
     		readcount = fread(buf,1, BLK_SIZE, device);
     		fwrite(buf, 1, readcount, destfile);
			i = readcount + i;
//			fprintf(logfile,"%d read (%d)\n",i,readcount);
		} while (readcount == BLK_SIZE);
	
		fclose(destfile);
	} else {
		fprintf(logfile,"Can't open file '%s'\n",path);
	}
}

void usage (void)
{
	fprintf(stderr,"usage: answer -D device -d destination -s source\n");
	exit(1); 
}

const char * argWithName (const char* aName)
{
	// '-D /dev/null -d 82834 -s 3305682834'
	int i = 0;
	int optionSeen = 0;
	int startpos = 0;

	for (i = 0; i < sizeof(tmpBuf);i++) {
		tmpBuf[i] = '\0';
	}

	for (i = 0; i<strlen(argbuf);i++) {
		if (optionSeen) {
			for (;(i<strlen(argbuf) && (argbuf[i] != ' '));i++) {
			}
			i++;
			startpos = i;

			for (;(i<strlen(argbuf) && (argbuf[i] != ' '));i++) {
			}
			strncpy(tmpBuf,&argbuf[startpos], i-startpos);

			return tmpBuf;
		}
		if (0 == strncmp(aName,&argbuf[i], strlen(aName))) {
			optionSeen = 1;
		}
	}

	usage();
	return NULL;
}

int main (int argc, const char *argv[]) {

	int i,pos = 0;
	extern char *optarg;
	extern int optind;
	int bflag, ch;
	char timeStr[50];
	char outfileName[1024] = "";
	char cmdStr[2048] = "";
	time_t now;

	now=time(NULL);

	strftime(timeStr,40,I4B_TIME_FORMAT,localtime(&now));

	logfile = fopen("/var/log/answer.log","a");

	fprintf(logfile,"%s Started\n",timeStr);

	pos=0;
	for (i=1;i<argc;i++) {
		sprintf(&argbuf[strlen(argbuf)],"%s ",argv[i]);
	}
	if (strlen(argbuf) > 2) {
		argbuf[strlen(argbuf)-1] = '\0';
	}


	device = fopen(argWithName("-D"),"r+");
	strcpy(destNum, argWithName("-d"));
	strcpy(srcNum, argWithName("-s"));

		fprintf(logfile,"device '%s'\n", argWithName("-D"));
		fprintf(logfile,"srcNum '%s'\n", srcNum);
		fprintf(logfile,"destNum '%s'\n", destNum);


	if (device) {

		strftime(timeStr,40,I4B_TIME_FORMAT,localtime(&now));

		sprintf(outfileName,"/var/isdn/%s_%s_%s", timeStr, srcNum, destNum);

		writeToPhone ("/usr/local/lib/isdn/msg.al");
		readFromPhone (outfileName);

		sprintf(cmdStr,"/bin/cat %s | %s | %s -t raw -U -b -r 8000 - -t .au %s.snd", outfileName, ALAWULAW, SOX, outfileName);
		fprintf(logfile,"%s\n",cmdStr);
		system(cmdStr);
		unlink(outfileName);

		fclose(device);
	} else {
		fprintf(logfile,"Can't open file '%s'\n",argWithName("-D"));
	}

	now=time(NULL);

	strftime(timeStr,40,I4B_TIME_FORMAT,localtime(&now));

	fprintf(logfile,"%s Done\n",timeStr);
	fclose(logfile);
    exit(0);       // insure the process exit status is 0
    return 0;      // ...and make main fit the ANSI spec.
}
