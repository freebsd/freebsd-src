#
#   [USEMAP]
#   
#                            Problem Report gnu/7821
#                                       
#   awk in free(): warning: chunk is already free.
#   
#   Confidential
#          no
#          
#   Severity
#          serious
#          
#   Priority
#          medium
#          
#   Responsible
#          freebsd-bugs@freebsd.org
#          
#   State
#          suspended
#          
#   Class
#          sw-bug
#          
#   Submitter-Id
#          current-users
#          
#   Arrival-Date
#          Thu Sep 3 10:30:00 PDT 1998
#          
#   Last-Modified
#          Thu Sep 17 02:04:26 PDT 1998
#          
#   Originator
#          Alexander Litvin archer@lucky.net
#          
#   Organization
#          
#
#Lucky Net ltd.
#
#   Release
#          FreeBSD 3.0-CURRENT i386
#          
#   Environment
#          
#
#FreeBSD grape.carrier.kiev.ua 3.0-CURRENT FreeBSD 3.0-CURRENT #121: Thu Sep  3
#1
#1:21:44 EEST 1998     archer@grape.carrier.kiev.ua:/usr/src/sys/compile/GRAPE
#i
#386
#
#   Description
#          
#
#The problem first appeared when GNU awk in 3.0-CURRENT was apgraded to
#3.0.3. I run C-News, which uses awk extensively. After awk apgrade C-News
#expire stopped to work. It appeared that some GNU awk 3.0.3 programms when
#given absolutely legitimate input fail, giving out a number of messages:
#
#awk in free(): warning: chunk is already free.
#
#   How-To-Repeat
#          
#
#Run the following awk program (it is cut out of C-News expire scripts).
#I was not able to cut it down more -- omitting some portions of the
#code (e.g. OFMT line), make error go away in this case, though it
#certainly does not fix awk.
#
#----------------cut-here----------------
#!/usr/bin/awk -f
BEGIN {
        OFMT = "%.12g"
        big = 99999999999
        lowest = big
        small = 0
        highest = small
}

$0 ~ /^[0-9]+$/ {
        if ($1 < lowest)
                lowest = $1
        if ($1 > highest)
                highest = $1
        next
}

$0 ~ /^[a-z]+/ {
        print dir, highest, lowest
        dir = $0
        lowest = big
        highest = small
}
#----------------cut-here----------------
#
#To get the error, just give this script the following input:
#----------------cut-here----------------
#a
#1
#b
#----------------cut-here----------------
#
#   Fix
#          
#
#I was not able to track the error in awk sources. As a workaround,
#I just reverted to GNU awk 2.15.5.
#
#   Audit-Trail
#          
#
#State-Changed-From-To: open-suspended
#State-Changed-By: phk
#State-Changed-When: Thu Sep 17 02:04:08 PDT 1998
#State-Changed-Why:
#reported to GNU maintainer.
#
#   Submit Followup
#     _________________________________________________________________
#                                      
#   
#    www@freebsd.org
