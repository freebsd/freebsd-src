# Sun Apr 25 13:28:58 IDT 1999
# from Juegen Khars.  This program should not core dump.
  function ReadPGM(f, d) {
print "ReadPGM"
    d[1] = 1
  }

  function WritePGM(f, d) {
print "WritePGM"
    d[1] = 0
  }

  BEGIN {
print "before ReadPGM"
    ReadPGM("", d)
print "after ReadPGM"
print "before WritePGM"
    WritePGM("", d)
print "after WritePGM"
  }
