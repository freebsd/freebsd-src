del LintOut.txt
echo Begin 64-bit lint >> LintOut.txt

"C:\Program Files\Lint\Lint-nt" +v std64.lnt +os(LintOut.txt) files.lnt

echo 64-bit lint completed >> LintOut.txt
echo -------------------------------------------- >> LintOut.txt
echo Begin 32-bit lint >> LintOut.txt

"C:\Program Files\Lint\Lint-nt" +v std32.lnt +os(LintOut.txt) files.lnt

echo 32-bit lint completed >> LintOut.txt
@echo off
echo ---
echo  Output placed in LintOut.txt

