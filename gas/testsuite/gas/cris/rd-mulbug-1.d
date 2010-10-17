#objdump: -s
#source: mulbug-err-1.s
#as: --em=criself --no-mul-bug-abort

# Check that we don't get any errors or messages with
# "--no-mul-bug-abort".  No checking for "--mul-bug-abort",
# though.

.*:     file format .*-cris

Contents of section \.text:
 0+ 114d0f05 014d2149                    .*
Contents of section \.text\.1:
 0+ 114d0f05 0149214d                    .*
Contents of section \.text\.2:
 0+ 11490f05 214d1149                    .*
Contents of section \.text\.3:
 0+ 114d2419 01490f05 0f050f05 0f050f05  .*
 0+10 0f050f05 0f050f05 0f050f05 0f050149  .*
 0+20 0149                                 .*
Contents of section \.text\.4:
 0+ 40d241d2 214d42d2 42d242d2 42d242d2  .*
 0+10 42d242d2 42d242d2 42d242d2 42d20f05  .*
 0+20 114d041d                             .*
