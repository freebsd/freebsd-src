# From dragon!gamgee.acad.emich.edu!dhw Tue Mar 18 01:12:15 1997
# Return-Path: <dragon!gamgee.acad.emich.edu!dhw>
# Message-ID: <m0w6owW-000IDSC@gamgee.acad.emich.edu>
# Date: Mon, 17 Mar 97 20:48 CST
# From: dhw@gamgee.acad.emich.edu (David H. West)
# To: arnold@gnu.ai.mit.edu
# Subject: gawk 3.0.2 bug report (cc of msg to bug-gnu-utils)
# Status: OR
# Content-Length: 869
# X-Lines: 20
# X-Display-Position: 2
# 
# Nature of bug: operation on a pipe side-effects a different pipe.
# Observed-With: gawk 3.0.2, Linux kernel 2.0.28
# Reproduce-By: running the following script, without and with the "close"
#               statement uncommented.
# -----------------cut here--------------------------
BEGIN {FILE1="test1"; FILE2="test2"; 
       print "1\n" > FILE1; close(FILE1);
       print "2\n" > FILE2; close(FILE2); 
       cmd1="cat " FILE1; cmd2="cat " FILE2;
       #end of preparing commands which give easily-predictable output

       while( (cmd1 | getline)==1) { #terminates as file has only 1 line
                                     #and we never close cmd1
          cmd2 | getline L; 
          #BUG: uncommenting the following line causes an infinite loop
          close(cmd2);
          print $0,L;
          }
      }
