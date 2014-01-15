#!/bin/sh
#
# $Id: tcgen.sh 2083 2011-10-27 04:41:39Z jkoshy $

usage()
{
    echo "Usage: tcgen.sh prog tcdir file [-S]"
}

if [ $# -lt 3 ]; then
    usage
    exit 1
fi

prog=$1
tcdir=$2
file=$3
rundir=`pwd`
if [ "$4" = "-S" ]; then
    ADD_S=yes
fi

cd "$tcdir"
rm -f tc
touch tc
echo "#!/bin/sh" > tc
echo "" >> tc
c=0
while [ 1 ]; do
    read line || break
    rlt=`echo "$line" | sed -e 's/ *-/@/g' -e 's/  */%/g'`
    if [ "$ADD_S" = yes ]; then
	rlt="@S${rlt}"
    fi
    $prog ${line} > "${rlt}.out" 2> "${rlt}.err"
    c=`expr $c + 1`
    echo "tp$c()" >> tc
    echo "{" >> tc
    echo "    run \"$rlt\"" >> tc
    echo "}" >> tc
    echo "" >> tc
done < ${rundir}/${file}
echo "" >> tc

echo "tet_startup=\"\"" >> tc
echo "tet_cleanup=\"cleanup\"" >> tc
echo "" >> tc

echo -n "iclist=\"" >> tc
i=1
while [ $i -le $c ]; do
   echo -n "ic${i}" >> tc
   if [ $i -ne $c ]; then
       echo -n " " >> tc
   fi
   i=`expr $i + 1`
done
echo "\"" >> tc
echo "" >> tc

i=1
while [ $i -le $c ]; do
    echo "ic${i}=\"tp${i}\"" >> tc
    i=`expr $i + 1`
done
echo "" >> tc

echo ". \$TET_SUITE_ROOT/ts/common/func.sh" >> tc
echo ". \$TET_ROOT/lib/xpg3sh/tcm.sh" >> tc

chmod +x tc
