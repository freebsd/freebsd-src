echo Setting up the environment for debugging vmlinux...\n
echo set remotedebug 0 \n
set remotedebug 0
echo cd arch/mips/kernel \n
cd arch/mips/kernel
echo target remote /dev/ttyS0 \n
target remote /dev/ttyS0
