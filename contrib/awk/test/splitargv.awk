BEGIN   {
          for (idx = 1; idx < ARGC; idx++)
            split(ARGV[idx], temp, ".");
        }
        {
          print $0;
        }
