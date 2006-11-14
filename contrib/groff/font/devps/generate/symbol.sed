#! /bin/sed -f
# Tweak the AFM file for the Symbol font.
/^C .*[ ;]N bracketlefttp[ ;]/bx
/^C .*[ ;]N bracketleftex[ ;]/bx
/^C .*[ ;]N bracketleftbt[ ;]/bx
/^C .*[ ;]N bracketrighttp[ ;]/bx
/^C .*[ ;]N bracketrightex[ ;]/bx
/^C .*[ ;]N bracketrightbt[ ;]/bx
/^C .*[ ;]N bracelefttp[ ;]/bx
/^C .*[ ;]N braceleftmid[ ;]/bx
/^C .*[ ;]N braceleftbt[ ;]/bx
/^C .*[ ;]N bracerighttp[ ;]/bx
/^C .*[ ;]N bracerightmid[ ;]/bx
/^C .*[ ;]N bracerightbt[ ;]/bx
/^C .*[ ;]N braceex[ ;]/bx
/^C .*[ ;]N parenleftex[ ;]/by
/^C .*[ ;]N parenrightex[ ;]/by
/^C .*[ ;]N parenleftbt[ ;]/bz
/^C .*[ ;]N parenrightbt[ ;]/bz
/^EndCharMetrics/a\
italicCorrection integral 67\
leftItalicCorrection integral 52\
subscriptCorrection integral -10
b
:x
s/B \([-0-9][0-9]*\) [-0-9][0-9]* \([-0-9][0-9]*\) [-0-9][0-9]*/B \1 -75 \2 925/
b
:y
s/B \([-0-9][0-9]*\) [-0-9][0-9]* \([-0-9][0-9]*\) [-0-9][0-9]*/B \1 -80 \2 920/
b
:z
s/B \([-0-9][0-9]*\) \([-0-9][0-9]*\) \([-0-9][0-9]*\) [-0-9][0-9]*/B \1 \2 \3 920/
b
