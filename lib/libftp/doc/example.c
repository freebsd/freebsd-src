
/* Include standard libftp's header */

#include <FtpLibrary.h>



main(int argc, char *argv[])
{

  FILE *input,*output;
  int c;


  if (argc<3)
    exit(fprintf(stderr,"Usage: %s input-file output-file\n",argv[0]));

  FtplibDebug(yes);

  if ((input=Ftpfopen(argv[1],"r"))==NULL)
    {
      perror(argv[1]);
      exit(1);
    }

  if ((output=Ftpfopen(argv[2],"w"))==NULL)
    {
      perror(argv[2]);
      exit(1);
    }

  while ( (c=getc(input)) != EOF &&  (putc(c,output)!=EOF) );

  if (ferror(input))
    {
      perror(argv[1]);
      exit(1);
    }

  if (ferror(output))
    {
      perror(argv[1]);
      exit(1);
    }

  Ftpfclose(input);
  Ftpfclose(output);

  exit(0);

}
