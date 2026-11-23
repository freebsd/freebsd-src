#! /usr/bin/env bash
# EXPAT TEST SCRIPT FOR W3C XML TEST SUITE
#
# This script can be used to exercise Expat against the
# w3c.org xml test suite, available from:
# https://www.w3.org/XML/Test/xmlts20020606.zip
#
# To run this script, first set XMLWF below so that xmlwf can be
# found, then set the output directory with OUTPUT.
#
# The script lists all test cases where Expat shows a discrepancy
# from the expected result. Test cases where only the canonical
# output differs are prefixed with "Output differs:", and a diff file
# is generated in the appropriate subdirectory under $OUTPUT.
#
# If there are output files provided, the script will use
# output from xmlwf and compare the desired output against it.
# However, one has to take into account that the canonical output
# produced by xmlwf conforms to an older definition of canonical XML
# and does not generate notation declarations.
#
#                          __  __            _
#                       ___\ \/ /_ __   __ _| |_
#                      / _ \\  /| '_ \ / _` | __|
#                     |  __//  \| |_) | (_| | |_
#                      \___/_/\_\ .__/ \__,_|\__|
#                               |_| XML parser
#
# Copyright (c) 2002-2004 Fred L. Drake, Jr. <fdrake@users.sourceforge.net>
# Copyright (c) 2002      Karl Waclawek <karl@waclawek.net>
# Copyright (c) 2008-2026 Sebastian Pipping <sebastian@pipping.org>
# Copyright (c) 2017      Rhodri James <rhodri@wildebeest.org.uk>
# Copyright (c) 2025      Hanno Böck <hanno@gentoo.org>
# Copyright (c) 2026      Huang Wenbin <huangwenbin@kylinos.cn>
# Licensed under the MIT license:
#
# Permission is  hereby granted,  free of charge,  to any  person obtaining
# a  copy  of  this  software   and  associated  documentation  files  (the
# "Software"),  to  deal in  the  Software  without restriction,  including
# without  limitation the  rights  to use,  copy,  modify, merge,  publish,
# distribute, sublicense, and/or sell copies of the Software, and to permit
# persons  to whom  the Software  is  furnished to  do so,  subject to  the
# following conditions:
#
# The above copyright  notice and this permission notice  shall be included
# in all copies or substantial portions of the Software.
#
# THE  SOFTWARE  IS  PROVIDED  "AS  IS",  WITHOUT  WARRANTY  OF  ANY  KIND,
# EXPRESS  OR IMPLIED,  INCLUDING  BUT  NOT LIMITED  TO  THE WARRANTIES  OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
# NO EVENT SHALL THE AUTHORS OR  COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
# DAMAGES OR  OTHER LIABILITY, WHETHER  IN AN  ACTION OF CONTRACT,  TORT OR
# OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
# USE OR OTHER DEALINGS IN THE SOFTWARE.

shopt -s nullglob

# Note: OUTPUT must terminate with the directory separator.
OUTPUT="$PWD/tests/out/"
TS="$PWD/tests/"

MYDIR="`dirname \"$0\"`"
cd "$MYDIR"
MYDIR="`pwd`"
XMLWF="${1:-`dirname \"$MYDIR\"`/xmlwf/xmlwf}"
# Unicode-aware diff utility
DIFF="${MYDIR}/udiffer.py"


# RunXmlwfNotWF file reldir
# reldir includes trailing slash
RunXmlwfNotWF() {
  file="$1"
  reldir="$2"
  if $XMLWF -p "$file" > /dev/null; then
      echo "Expected not well-formed: $reldir$file"
      return 1
  else
      return 0
  fi 
}

# RunXmlwfWF file reldir
# reldir includes trailing slash
RunXmlwfWF() {
  file="$1"
  reldir="$2"
  $XMLWF -p -N -d "$OUTPUT$reldir" "$file" > outfile || return $?
  read outdata < outfile 
  if test "$outdata" = "" ; then 
      if [ -f "out/$file" ] ; then 
          $DIFF "$OUTPUT$reldir$file" "out/$file" > outfile 
          if [ -s outfile ] ; then 
              cp outfile "$OUTPUT$reldir$file.diff"
              echo "Output differs: $reldir$file"
              return 1
          fi 
      fi 
      return 0
  else 
      echo "In $reldir: $outdata"
      return 1
  fi 
}

SUCCESS=0
ERROR=0

UpdateStatus() {
  if [ "$1" -eq 0 ] ; then
    SUCCESS=`expr $SUCCESS + 1`
  else
    ERROR=`expr $ERROR + 1`
  fi
}

# These these tests are explicitly marked as `EDITION="1 2 3 4"` which means that they
# are not suitable for XML 1.0r5.
cd "$TS/xmlconf"
incompatible_tests=(
  ibm/not-wf/P85/ibm85n03.xml
  ibm/not-wf/P85/ibm85n04.xml
  ibm/not-wf/P85/ibm85n05.xml
  ibm/not-wf/P85/ibm85n06.xml
  ibm/not-wf/P85/ibm85n07.xml
  ibm/not-wf/P85/ibm85n08.xml
  ibm/not-wf/P85/ibm85n09.xml
  ibm/not-wf/P85/ibm85n100.xml
  ibm/not-wf/P85/ibm85n101.xml
  ibm/not-wf/P85/ibm85n102.xml
  ibm/not-wf/P85/ibm85n103.xml
  ibm/not-wf/P85/ibm85n104.xml
  ibm/not-wf/P85/ibm85n105.xml
  ibm/not-wf/P85/ibm85n106.xml
  ibm/not-wf/P85/ibm85n107.xml
  ibm/not-wf/P85/ibm85n108.xml
  ibm/not-wf/P85/ibm85n109.xml
  ibm/not-wf/P85/ibm85n10.xml
  ibm/not-wf/P85/ibm85n110.xml
  ibm/not-wf/P85/ibm85n111.xml
  ibm/not-wf/P85/ibm85n112.xml
  ibm/not-wf/P85/ibm85n113.xml
  ibm/not-wf/P85/ibm85n114.xml
  ibm/not-wf/P85/ibm85n115.xml
  ibm/not-wf/P85/ibm85n116.xml
  ibm/not-wf/P85/ibm85n117.xml
  ibm/not-wf/P85/ibm85n118.xml
  ibm/not-wf/P85/ibm85n119.xml
  ibm/not-wf/P85/ibm85n11.xml
  ibm/not-wf/P85/ibm85n120.xml
  ibm/not-wf/P85/ibm85n121.xml
  ibm/not-wf/P85/ibm85n122.xml
  ibm/not-wf/P85/ibm85n123.xml
  ibm/not-wf/P85/ibm85n124.xml
  ibm/not-wf/P85/ibm85n125.xml
  ibm/not-wf/P85/ibm85n126.xml
  ibm/not-wf/P85/ibm85n127.xml
  ibm/not-wf/P85/ibm85n128.xml
  ibm/not-wf/P85/ibm85n129.xml
  ibm/not-wf/P85/ibm85n12.xml
  ibm/not-wf/P85/ibm85n130.xml
  ibm/not-wf/P85/ibm85n131.xml
  ibm/not-wf/P85/ibm85n132.xml
  ibm/not-wf/P85/ibm85n133.xml
  ibm/not-wf/P85/ibm85n134.xml
  ibm/not-wf/P85/ibm85n135.xml
  ibm/not-wf/P85/ibm85n136.xml
  ibm/not-wf/P85/ibm85n137.xml
  ibm/not-wf/P85/ibm85n138.xml
  ibm/not-wf/P85/ibm85n139.xml
  ibm/not-wf/P85/ibm85n13.xml
  ibm/not-wf/P85/ibm85n140.xml
  ibm/not-wf/P85/ibm85n141.xml
  ibm/not-wf/P85/ibm85n142.xml
  ibm/not-wf/P85/ibm85n143.xml
  ibm/not-wf/P85/ibm85n144.xml
  ibm/not-wf/P85/ibm85n145.xml
  ibm/not-wf/P85/ibm85n146.xml
  ibm/not-wf/P85/ibm85n147.xml
  ibm/not-wf/P85/ibm85n148.xml
  ibm/not-wf/P85/ibm85n149.xml
  ibm/not-wf/P85/ibm85n14.xml
  ibm/not-wf/P85/ibm85n150.xml
  ibm/not-wf/P85/ibm85n151.xml
  ibm/not-wf/P85/ibm85n152.xml
  ibm/not-wf/P85/ibm85n153.xml
  ibm/not-wf/P85/ibm85n154.xml
  ibm/not-wf/P85/ibm85n155.xml
  ibm/not-wf/P85/ibm85n156.xml
  ibm/not-wf/P85/ibm85n157.xml
  ibm/not-wf/P85/ibm85n158.xml
  ibm/not-wf/P85/ibm85n159.xml
  ibm/not-wf/P85/ibm85n15.xml
  ibm/not-wf/P85/ibm85n160.xml
  ibm/not-wf/P85/ibm85n161.xml
  ibm/not-wf/P85/ibm85n162.xml
  ibm/not-wf/P85/ibm85n163.xml
  ibm/not-wf/P85/ibm85n164.xml
  ibm/not-wf/P85/ibm85n165.xml
  ibm/not-wf/P85/ibm85n166.xml
  ibm/not-wf/P85/ibm85n167.xml
  ibm/not-wf/P85/ibm85n168.xml
  ibm/not-wf/P85/ibm85n169.xml
  ibm/not-wf/P85/ibm85n16.xml
  ibm/not-wf/P85/ibm85n170.xml
  ibm/not-wf/P85/ibm85n171.xml
  ibm/not-wf/P85/ibm85n172.xml
  ibm/not-wf/P85/ibm85n173.xml
  ibm/not-wf/P85/ibm85n174.xml
  ibm/not-wf/P85/ibm85n175.xml
  ibm/not-wf/P85/ibm85n176.xml
  ibm/not-wf/P85/ibm85n177.xml
  ibm/not-wf/P85/ibm85n178.xml
  ibm/not-wf/P85/ibm85n179.xml
  ibm/not-wf/P85/ibm85n17.xml
  ibm/not-wf/P85/ibm85n180.xml
  ibm/not-wf/P85/ibm85n181.xml
  ibm/not-wf/P85/ibm85n182.xml
  ibm/not-wf/P85/ibm85n183.xml
  ibm/not-wf/P85/ibm85n184.xml
  ibm/not-wf/P85/ibm85n185.xml
  ibm/not-wf/P85/ibm85n186.xml
  ibm/not-wf/P85/ibm85n187.xml
  ibm/not-wf/P85/ibm85n188.xml
  ibm/not-wf/P85/ibm85n189.xml
  ibm/not-wf/P85/ibm85n18.xml
  ibm/not-wf/P85/ibm85n190.xml
  ibm/not-wf/P85/ibm85n191.xml
  ibm/not-wf/P85/ibm85n192.xml
  ibm/not-wf/P85/ibm85n193.xml
  ibm/not-wf/P85/ibm85n194.xml
  ibm/not-wf/P85/ibm85n195.xml
  ibm/not-wf/P85/ibm85n196.xml
  ibm/not-wf/P85/ibm85n197.xml
  ibm/not-wf/P85/ibm85n198.xml
  ibm/not-wf/P85/ibm85n19.xml
  ibm/not-wf/P85/ibm85n20.xml
  ibm/not-wf/P85/ibm85n21.xml
  ibm/not-wf/P85/ibm85n22.xml
  ibm/not-wf/P85/ibm85n23.xml
  ibm/not-wf/P85/ibm85n24.xml
  ibm/not-wf/P85/ibm85n25.xml
  ibm/not-wf/P85/ibm85n26.xml
  ibm/not-wf/P85/ibm85n27.xml
  ibm/not-wf/P85/ibm85n28.xml
  ibm/not-wf/P85/ibm85n29.xml
  ibm/not-wf/P85/ibm85n30.xml
  ibm/not-wf/P85/ibm85n31.xml
  ibm/not-wf/P85/ibm85n32.xml
  ibm/not-wf/P85/ibm85n33.xml
  ibm/not-wf/P85/ibm85n34.xml
  ibm/not-wf/P85/ibm85n35.xml
  ibm/not-wf/P85/ibm85n36.xml
  ibm/not-wf/P85/ibm85n37.xml
  ibm/not-wf/P85/ibm85n38.xml
  ibm/not-wf/P85/ibm85n39.xml
  ibm/not-wf/P85/ibm85n40.xml
  ibm/not-wf/P85/ibm85n41.xml
  ibm/not-wf/P85/ibm85n42.xml
  ibm/not-wf/P85/ibm85n43.xml
  ibm/not-wf/P85/ibm85n44.xml
  ibm/not-wf/P85/ibm85n45.xml
  ibm/not-wf/P85/ibm85n46.xml
  ibm/not-wf/P85/ibm85n47.xml
  ibm/not-wf/P85/ibm85n48.xml
  ibm/not-wf/P85/ibm85n49.xml
  ibm/not-wf/P85/ibm85n50.xml
  ibm/not-wf/P85/ibm85n51.xml
  ibm/not-wf/P85/ibm85n52.xml
  ibm/not-wf/P85/ibm85n53.xml
  ibm/not-wf/P85/ibm85n54.xml
  ibm/not-wf/P85/ibm85n55.xml
  ibm/not-wf/P85/ibm85n56.xml
  ibm/not-wf/P85/ibm85n57.xml
  ibm/not-wf/P85/ibm85n58.xml
  ibm/not-wf/P85/ibm85n59.xml
  ibm/not-wf/P85/ibm85n60.xml
  ibm/not-wf/P85/ibm85n61.xml
  ibm/not-wf/P85/ibm85n62.xml
  ibm/not-wf/P85/ibm85n63.xml
  ibm/not-wf/P85/ibm85n64.xml
  ibm/not-wf/P85/ibm85n65.xml
  ibm/not-wf/P85/ibm85n66.xml
  ibm/not-wf/P85/ibm85n67.xml
  ibm/not-wf/P85/ibm85n68.xml
  ibm/not-wf/P85/ibm85n69.xml
  ibm/not-wf/P85/ibm85n70.xml
  ibm/not-wf/P85/ibm85n71.xml
  ibm/not-wf/P85/ibm85n72.xml
  ibm/not-wf/P85/ibm85n73.xml
  ibm/not-wf/P85/ibm85n74.xml
  ibm/not-wf/P85/ibm85n75.xml
  ibm/not-wf/P85/ibm85n76.xml
  ibm/not-wf/P85/ibm85n77.xml
  ibm/not-wf/P85/ibm85n78.xml
  ibm/not-wf/P85/ibm85n79.xml
  ibm/not-wf/P85/ibm85n80.xml
  ibm/not-wf/P85/ibm85n81.xml
  ibm/not-wf/P85/ibm85n82.xml
  ibm/not-wf/P85/ibm85n83.xml
  ibm/not-wf/P85/ibm85n84.xml
  ibm/not-wf/P85/ibm85n85.xml
  ibm/not-wf/P85/ibm85n86.xml
  ibm/not-wf/P85/ibm85n87.xml
  ibm/not-wf/P85/ibm85n88.xml
  ibm/not-wf/P85/ibm85n89.xml
  ibm/not-wf/P85/ibm85n90.xml
  ibm/not-wf/P85/ibm85n91.xml
  ibm/not-wf/P85/ibm85n92.xml
  ibm/not-wf/P85/ibm85n93.xml
  ibm/not-wf/P85/ibm85n94.xml
  ibm/not-wf/P85/ibm85n95.xml
  ibm/not-wf/P85/ibm85n96.xml
  ibm/not-wf/P85/ibm85n97.xml
  ibm/not-wf/P85/ibm85n98.xml
  ibm/not-wf/P85/ibm85n99.xml
  ibm/not-wf/P86/ibm86n01.xml
  ibm/not-wf/P86/ibm86n02.xml
  ibm/not-wf/P86/ibm86n03.xml
  ibm/not-wf/P86/ibm86n04.xml
  ibm/not-wf/P87/ibm87n01.xml
  ibm/not-wf/P87/ibm87n02.xml
  ibm/not-wf/P87/ibm87n03.xml
  ibm/not-wf/P87/ibm87n04.xml
  ibm/not-wf/P87/ibm87n05.xml
  ibm/not-wf/P87/ibm87n06.xml
  ibm/not-wf/P87/ibm87n07.xml
  ibm/not-wf/P87/ibm87n08.xml
  ibm/not-wf/P87/ibm87n09.xml
  ibm/not-wf/P87/ibm87n10.xml
  ibm/not-wf/P87/ibm87n11.xml
  ibm/not-wf/P87/ibm87n12.xml
  ibm/not-wf/P87/ibm87n13.xml
  ibm/not-wf/P87/ibm87n14.xml
  ibm/not-wf/P87/ibm87n15.xml
  ibm/not-wf/P87/ibm87n16.xml
  ibm/not-wf/P87/ibm87n17.xml
  ibm/not-wf/P87/ibm87n18.xml
  ibm/not-wf/P87/ibm87n19.xml
  ibm/not-wf/P87/ibm87n20.xml
  ibm/not-wf/P87/ibm87n21.xml
  ibm/not-wf/P87/ibm87n22.xml
  ibm/not-wf/P87/ibm87n23.xml
  ibm/not-wf/P87/ibm87n24.xml
  ibm/not-wf/P87/ibm87n25.xml
  ibm/not-wf/P87/ibm87n26.xml
  ibm/not-wf/P87/ibm87n27.xml
  ibm/not-wf/P87/ibm87n28.xml
  ibm/not-wf/P87/ibm87n29.xml
  ibm/not-wf/P87/ibm87n30.xml
  ibm/not-wf/P87/ibm87n31.xml
  ibm/not-wf/P87/ibm87n32.xml
  ibm/not-wf/P87/ibm87n33.xml
  ibm/not-wf/P87/ibm87n34.xml
  ibm/not-wf/P87/ibm87n35.xml
  ibm/not-wf/P87/ibm87n36.xml
  ibm/not-wf/P87/ibm87n37.xml
  ibm/not-wf/P87/ibm87n38.xml
  ibm/not-wf/P87/ibm87n39.xml
  ibm/not-wf/P87/ibm87n40.xml
  ibm/not-wf/P87/ibm87n41.xml
  ibm/not-wf/P87/ibm87n42.xml
  ibm/not-wf/P87/ibm87n43.xml
  ibm/not-wf/P87/ibm87n44.xml
  ibm/not-wf/P87/ibm87n45.xml
  ibm/not-wf/P87/ibm87n46.xml
  ibm/not-wf/P87/ibm87n47.xml
  ibm/not-wf/P87/ibm87n48.xml
  ibm/not-wf/P87/ibm87n49.xml
  ibm/not-wf/P87/ibm87n50.xml
  ibm/not-wf/P87/ibm87n51.xml
  ibm/not-wf/P87/ibm87n52.xml
  ibm/not-wf/P87/ibm87n53.xml
  ibm/not-wf/P87/ibm87n54.xml
  ibm/not-wf/P87/ibm87n55.xml
  ibm/not-wf/P87/ibm87n56.xml
  ibm/not-wf/P87/ibm87n57.xml
  ibm/not-wf/P87/ibm87n58.xml
  ibm/not-wf/P87/ibm87n59.xml
  ibm/not-wf/P87/ibm87n60.xml
  ibm/not-wf/P87/ibm87n61.xml
  ibm/not-wf/P87/ibm87n62.xml
  ibm/not-wf/P87/ibm87n63.xml
  ibm/not-wf/P87/ibm87n64.xml
  ibm/not-wf/P87/ibm87n66.xml
  ibm/not-wf/P87/ibm87n67.xml
  ibm/not-wf/P87/ibm87n68.xml
  ibm/not-wf/P87/ibm87n69.xml
  ibm/not-wf/P87/ibm87n70.xml
  ibm/not-wf/P87/ibm87n71.xml
  ibm/not-wf/P87/ibm87n72.xml
  ibm/not-wf/P87/ibm87n73.xml
  ibm/not-wf/P87/ibm87n74.xml
  ibm/not-wf/P87/ibm87n75.xml
  ibm/not-wf/P87/ibm87n76.xml
  ibm/not-wf/P87/ibm87n77.xml
  ibm/not-wf/P87/ibm87n78.xml
  ibm/not-wf/P87/ibm87n79.xml
  ibm/not-wf/P87/ibm87n80.xml
  ibm/not-wf/P87/ibm87n81.xml
  ibm/not-wf/P87/ibm87n82.xml
  ibm/not-wf/P87/ibm87n83.xml
  ibm/not-wf/P87/ibm87n84.xml
  ibm/not-wf/P87/ibm87n85.xml
  ibm/not-wf/P88/ibm88n03.xml
  ibm/not-wf/P88/ibm88n04.xml
  ibm/not-wf/P88/ibm88n05.xml
  ibm/not-wf/P88/ibm88n06.xml
  ibm/not-wf/P88/ibm88n08.xml
  ibm/not-wf/P88/ibm88n09.xml
  ibm/not-wf/P88/ibm88n10.xml
  ibm/not-wf/P88/ibm88n11.xml
  ibm/not-wf/P88/ibm88n12.xml
  ibm/not-wf/P88/ibm88n13.xml
  ibm/not-wf/P88/ibm88n14.xml
  ibm/not-wf/P88/ibm88n15.xml
  ibm/not-wf/P88/ibm88n16.xml
  ibm/not-wf/P89/ibm89n03.xml
  ibm/not-wf/P89/ibm89n04.xml
  ibm/not-wf/P89/ibm89n05.xml
  ibm/not-wf/P89/ibm89n06.xml
  ibm/not-wf/P89/ibm89n07.xml
  ibm/not-wf/P89/ibm89n08.xml
  ibm/not-wf/P89/ibm89n09.xml
  ibm/not-wf/P89/ibm89n10.xml
  ibm/not-wf/P89/ibm89n11.xml
  ibm/not-wf/P89/ibm89n12.xml
  xmltest/not-wf/sa/140.xml
  xmltest/not-wf/sa/141.xml
  eduni/xml-1.1/014.xml
  eduni/xml-1.1/016.xml
  eduni/xml-1.1/019.xml
  eduni/errata-4e/008.xml
)
rm -f "${incompatible_tests[@]}"


##########################
# well-formed test cases #
##########################

cd "$TS/xmlconf"
for xmldir in ibm/valid/P* \
              ibm/invalid/P* \
              xmltest/valid/ext-sa \
              xmltest/valid/not-sa \
              xmltest/invalid \
              xmltest/invalid/not-sa \
              xmltest/valid/sa \
              sun/valid \
              sun/invalid ; do
  cd "$TS/xmlconf/$xmldir"
  mkdir -p "$OUTPUT$xmldir"
  for xmlfile in $(ls -1 *.xml | sort -d) ; do
      [[ -f "$xmlfile" ]] || continue
      RunXmlwfWF "$xmlfile" "$xmldir/"
      UpdateStatus $?
  done
  rm -f outfile
done

cd "$TS/xmlconf/oasis"
mkdir -p "$OUTPUT"oasis
for xmlfile in *pass*.xml ; do
    RunXmlwfWF "$xmlfile" "oasis/"
    UpdateStatus $?
done
rm outfile

##############################
# not well-formed test cases #
##############################

cd "$TS/xmlconf"
for xmldir in ibm/not-wf/P* \
              ibm/not-wf/p28a \
              ibm/not-wf/misc \
              xmltest/not-wf/ext-sa \
              xmltest/not-wf/not-sa \
              xmltest/not-wf/sa \
              sun/not-wf ; do
  cd "$TS/xmlconf/$xmldir"
  for xmlfile in *.xml ; do
      RunXmlwfNotWF "$xmlfile" "$xmldir/"
      UpdateStatus $?
  done
done

cd "$TS/xmlconf/oasis"
for xmlfile in *fail*.xml ; do
    RunXmlwfNotWF "$xmlfile" "oasis/"
    UpdateStatus $?
done

echo "Passed: $SUCCESS"
echo "Failed: $ERROR"
test "$ERROR" -eq 0
