#!/bin/sh
#
# uuspeed - a script to parse a Taylor UUCP Stats file into pretty results.
# Zacharias J. Beckman.

grep bytes /usr/spool/uucp/Stats | grep -v 'bytes 0.00 secs' | grep -v 'failed after' | tail -80 | \
gawk '
  BEGIN {
    printf("          UUCP transmission history:\n");
    format=" %8d bytes %8s(%8s) in %7.2f sec = %5.0f baud, %4.1fK / min\n";
    average=0.01;
    samples=0;
  }

  {
  if ($6 > 100) {
      printf (format, $6, $5, $2, $9, $6/$9*10, ($6/$9*60)/1000);

      average += ($6/$9*10);
      samples += 1;
    }
  }

  END {
    printf ("          average speed %d baud\n", average/samples);
  }
'
