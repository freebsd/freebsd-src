/*
 *      Trivial program to load VT220 Function keys with strings,
 *      note that the values only get sent when the key is shifted
 *      (shoulda been an option to flip the shift set like the Z19!)
 *
 *      Typing no args gives help, basically pairs of keyname/value
 *      strings.
 *
 *      Author, Author: Barry Shein, Boston University
 *
 * HISTORY
  {1}   30-Oct-85  Kenneth J. Lester (ken) at ektools

        Added the necessary code to read an initialization file.  This
        should make it easier to used this program.  Also added code
        that will set-up the terminal in vt200 (this saves the user the
        trouble of checking if the set-up is in vt200).

        Restructed  the  main  function  to  use   getopt,  for  argument
        processing.

        Alterated usage function  to include  new "i"  option (init file)


 	-hm	minor modifications for pcvt 2.0 release

*/

#include <stdio.h>
#include <ctype.h>

/*
 *      The default toupper() macro is stupid, will toupper anything
 */

#ifdef toupper
#undef toupper
#endif
#define toupper(c) (islower(c) ? ((c)-' ') : c)

#define VT200_7BIT 1
#define ESC 033
#define INITFILE ".vt220rc"

struct keynames {
  char *name ;
  char *string ;
} keys[] = {
  "F6", "17",
  "F7", "18",
  "F8", "19",
  "F9", "20",
  "F10", "21",
  "F11", "23",
  "ESC", "23",
  "F12", "24",
  "BS", "24",
  "F13", "25",
  "LF", "25",
  "F14", "26",
  "HELP", "28",
  "DO", "29",
  "F17", "31",
  "F18", "32",
  "F19", "33",
  "F20", "34",
    NULL, NULL
};

char prog[BUFSIZ];

main(argc,argv)
        int argc;
        char *argv[];
{
        /* these are defined in the getopt routine                       */
        extern char *optarg;    /* argument give to an option            */
        extern int  optind;     /* argv index after option processing    */

        int option;             /* option character returned by getopt   */
        int initf = 0;          /* read initialization file              */
        int lockf = 0;          /* lock keys after loading strings       */
        int clearf = 0;         /* clear all keys before loading strings */
	char *strcpy();

        (void) strcpy(prog, *argv);  /* store program name               */

        if(argc == 1) usage();  /* program requires options              */

        /* get options */
        while ((option = getopt(argc, argv, "cli")) != -1)
        switch(option)
        {
                case 'c' :
                        clearf++;
                        break;
                case 'l' :
                        lockf++;
                        break;
                case 'i' :
                        initf++;
                        break;
                case '?' :
                        usage();
          }

        if (VT200_7BIT)
                printf("\033[62;1\"p");    /* vt200 7 bits */
        else
                printf("\033[62;2\"p");    /* vt200 8 bits */

        if(clearf) clearkeys();

        if (initf) getinit();

        /* process {key, key string} pairs.  Note optind is index to argv
           for first pair.  By adding 1 to optind insures that a pair exists
           i.e. the last key has a key string.                             */

        while(optind + 1 < argc)
        {
                dokey(argv[optind], argv[optind+1]);
                optind += 2;
        }

        if(lockf) lockkeys();

        exit(0);
}

/****************************************************************************/

/*
 *      Load the VT220 SHIFT-FNKEY value, the basic pattern is
 *              "\EP1;1|"+KEYNAME+"/"+VAL_AS_HEX+"\E\\"
 *      that is, literally what is in quotes (w/o quotes) then the
 *      name of the key from the keytable above (a numeric string)
 *      then a slash, then the string value as hex pairs then ESC-BACKSLASH
 *
 *      Note: you can gang together key defns with semicolons but that
 *      would complicate things, especially error handling, so do it all
 *      for each pair, who cares, really.
 */

dokey(nm,val) char *nm, *val;
{
        register char *scr;
        register struct keynames *kp;

        for(scr = nm; *scr = toupper(*scr); scr++)
                        ;
        for(kp = keys; kp->name != NULL; kp++)
          if(strcmp(nm,kp->name) == 0) {
            printf("%cP1;1|%s/",ESC,kp->string);
            while(*val) printf("%02x",*val++);
            printf("%c\\",ESC);
            fflush(stdout);
            return;
        }
        fprintf(stderr,"Bad key name: %s\n",nm);
        usage();        /* bad key name, give up */
}

/****************************************************************************/

clearkeys()
{
        printf("%cP0;1|%c\\",ESC,ESC);
        fflush(stdout);
}

/****************************************************************************/

lockkeys()
{
        printf("%cP1;0|%c\\",ESC,ESC);
        fflush(stdout);
}

/****************************************************************************/

usage()
{
        int i;

        fprintf(stderr,"Usage: %s [-cil] [keyname string keyname string...]\n\n",prog);
        fprintf(stderr,"The following options are available\n");
        fprintf(stderr,"\t-c\tclears keys first\n");
        fprintf(stderr,"\t-l\t[sets then] locks further setting\n");
        fprintf(stderr,"\t-i\tfirst read initialization file $HOME/%s\n",INITFILE);
        fprintf(stderr,"(note that the only way to unlock is via Set-Up)\n\n");
        fprintf(stderr,"Keyname is one of:\n\t");
        for(i=0; keys[i].name != NULL; i++)
                fprintf(stderr,"%s ",keys[i].name);
        fprintf(stderr,"\nKeyname is SHIFTED function key that sends the string\n\n");
        fprintf(stderr,"Strings may need quoting to protect from shell\n");
        fprintf(stderr,"You must specify an option or key,string pairs\n\n");
        exit(1);
}

/****************************************************************************/

/* This routine process the INITFILE.  This file expects lines in the format

                <ws> keyname ws string

   Where ws is white space (spaces or tabs) and <ws> is optional white space.
   The string may include spaces or tabs and need not be quoted.  If the
   string has the sequence of "\n" then a newline character is included in
   the string.

   examples:

        F6      ls -lg\n
        F7      uulog -s

*/

#include <sys/types.h>
#include <sys/stat.h>

getinit()
{
        char *home;             /* user's home directory                */
        char path[BUFSIZ];      /* full path name of init file          */
        char buf[BUFSIZ];       /* buffer to hold 1 line from init file */
        char key[BUFSIZ];       /* buffer, to hold specified fcn key    */
        char keystr[BUFSIZ];    /* string associated with fcn key       */
        char *ptr;              /* pointer to transverse buf            */
        int i, j;               /* array indices                        */
        int statflag;           /* whether init file is regular & readable */
        struct stat statbuf;    /* stat of the init file                */
        FILE *fp;               /* file pointer to init file            */

        /* system calls and subroutines */
        FILE *fopen();
        char *strcpy();
        char *strcat();
        char *fgets();
        char *getenv();

        /* construct full path name for init file */
        home = getenv("HOME");
        (void) strcpy(path, home);
        (void) strcat(path,"/");
        (void) strcat(path,INITFILE);

        /* check status if init file    */
        if (stat(path, &statbuf) != -1)
        {
            statflag = statbuf.st_mode & S_IFREG && statbuf.st_mode & S_IREAD;
            if (!statflag || (fp = fopen(path, "r")) == NULL)
            {
                fprintf(stderr, "couldn't open initalization file: %s\n", path);
                exit(1);
            }

            /* process lines from init file */
            while (fgets(buf, BUFSIZ, fp) != NULL)
            {
                /* variable initializations */
                i = 0; j = 0;
                key[0] = '\0'; keystr[0] = '\0';
                ptr = buf;

                while (*ptr == ' ' || *ptr == '\t') ptr++; /*skip whitespace*/

		if (*ptr == '\n') break;   /* we hit an emtpy line          */

                while (!isspace(*ptr) && *ptr != '\0')     /* get keyname   */
                    key[i++] = *ptr++;
                key[i] = '\0'; /* place EOS in buffer */

                while (*ptr == ' ' || *ptr == '\t') ptr++; /*skip whitespace*/

                while (*ptr != '\n' && *ptr != '\0')       /* get string    */
                {
                    /* check if string is to include newline i.e. \n        */
                    if (*ptr == '\\' && *(ptr+1) == 'n')
                    {
                          keystr[j] = '\012';
                          ptr++;
                    }
                    else
                          keystr[j] = *ptr;
                    j++; ptr++;
                }
                keystr[j] = '\0';     /* place EOS in buffer  */
                dokey(key, keystr);   /* load key with string */
            }
        }
        else
        {
            fprintf(stderr, "init file %s not found\n\n", path);
            usage();
        }
}
