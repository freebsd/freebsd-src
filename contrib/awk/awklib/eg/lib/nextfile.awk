# nextfile --- skip remaining records in current file
# correctly handle successive occurrences of the same file
#
# Arnold Robbins, arnold@gnu.org, Public Domain
# May, 1993

# this should be read in before the "main" awk program

function nextfile()   { _abandon_ = FILENAME; next }

_abandon_ == FILENAME {
      if (FNR == 1)
          _abandon_ = ""
      else
          next
}
