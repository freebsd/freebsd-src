
#include <GetOpt.h>

int
main (int argc, char **argv)
{
  int digit_optind = 0;
  GetOpt getopt (argc, argv, "abc:d:0123456789");

  while (1)
    {
      int this_option_optind = getopt.optind;
      int ch;
      if ((ch = getopt ()) == EOF)
        break;
      char c = char(ch);
      switch (c)
        {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
          if (digit_optind != 0 && digit_optind != this_option_optind)
            printf ("digits occur in two different argv-elements.\n");
          digit_optind = this_option_optind;
          printf ("option %c\n", c);
          break;

        case 'a':
          printf ("option a\n");
          break;

        case 'b':
          printf ("option b\n");
          break;

        case 'c':
          printf ("option c with value `%s'\n", getopt.optarg);
          break;

        case '?':
          break;

        default:
          printf ("?? getopt returned character code 0%o ??\n", c);
        }
    }

  if (getopt.optind < argc)
    {
      printf ("non-option ARGV-elements: ");
      while (getopt.optind < argc)
        printf ("%s ", argv[getopt.optind++]);
      printf ("\n");
    }

  return 0;
}

