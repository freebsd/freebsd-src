#!/bin/sh
#
# uuq - a script to examine and display the Taylor spool directory contents.
#       note - uses the uuname script or similar functionality.
# Zacharias Beckman

SPOOLDIR="/usr/spool/uucp"
SYSTEMS=`uuname`
TMPFILE="/tmp/uuq.tmp"
FORSYSTEM=""
DELETE=""
LONG=0
SINGLE=0

while [ "$1" != "" ]
do
  case $1 in
    -l) LONG=1
        shift
        ;;
    -s) shift
        SYSTEMS=$argv[1]
        SINGLE=1
        shift
        ;;
    -d) shift
        DELETE=$argv[1]
        shift
        ;;
    -h) echo "uuq: usage uuq [options]"
        echo "     -l    long listing (may take a while)"
        echo "     -s n  run uuq only for system n"
        echo "     -d n  delete item n from the queue (required -s)"
        exit 1
            ;;
        *)  echo "uuq: invalid option"
            exit 1
            ;;
  esac
done

if [ "${DELETE}" != "" ] && [ ${SINGLE} != 1 ] ; then
  echo "uuq: you must specify a system to delete the job from:"
  echo "     uuq -s wizard -d D.0004"
  exit 1
fi

cd ${SPOOLDIR}

# if we are deleting a job, then do that first and exit without showing
# any other queue information

if [ "${DELETE}" != "" ] ; then
  if [ -d ${SYSTEMS}/D. ] ; then
    cd ${SYSTEMS}/C.
    PACKET=${DELETE}
    if [ -f ${PACKET} ] ; then
      EXFILE=../D.X/`awk '{if (NR == 2) print $2}' ${PACKET}`
      DFILE=../D./`awk '{if (NR == 1) print $2}' ${PACKET}`
      echo "deleting job ${PACKET}"
      rm ${PACKET}
      rm ${EXFILE}
      rm ${DFILE}
    else
      echo "uuq: job ${PACKET} not found"
      exit 1
    fi
  else
    echo "uuq: system ${SYSTEMS} not found"
  fi

  exit 1
fi

# use the 'uuname' script to obtain a list of systems for the 'sys' file,
# then step through each directory looking for appropriate information.

if [ ${LONG} -gt 0 ] ; then
  echo "system"
  echo -n "job#    act size       command"
fi

for DESTSYSTEM in ${SYSTEMS} ; do
  # if there is an existing directory for the named system, cd into it and
  # "do the right thing."

  if [ -d ${DESTSYSTEM} ] ; then
    cd ${DESTSYSTEM}/C.

    PACKET=`ls`

    if [ "${PACKET}" != "" ] ; then
      # if a long listing has been required, extra information is printed

      echo ""
      echo "${DESTSYSTEM}:"

      # now each packet must be examined and appropriate information is
      # printed for this system

      if [ ${LONG} -gt 0 ] ; then
        for PACKET in * ; do
          EXFILE=../D.X/`awk '{if (NR == 2) print $2}' ${PACKET}`
          DFILE=../D./`awk '{if (NR == 1) print $2}' ${PACKET}`
          echo -n "${PACKET} " > ${TMPFILE}
          gawk '{if (NR == 2) printf(" %s  ", $1);}' ${PACKET} >> ${TMPFILE}
          ls -l ${DFILE}|awk '{printf("%-10d ", $4)}' >> ${TMPFILE}
          if [ -f ${EXFILE} ] ; then
            gawk '/U / {printf("(%s)", $2);}\
                  /C / {print substr($0,2,length($0));}' ${EXFILE} >> ${TMPFILE}
          else
            echo "---" >> ${TMPFILE}
          fi

          cat ${TMPFILE}
        done
        cat ${SPOOLDIR}/.Status/${DESTSYSTEM}
      else
        ls
      fi
    fi
  fi

  cd ${SPOOLDIR}
done
