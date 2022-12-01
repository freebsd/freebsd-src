echo "Copying kernel to fbd" &&
rsync  ../obj/home/allison/freebsd/src/arm64.aarch64/sys/GENERIC/kernel fbd:~/qemu/fbd_update_kernel.img --progress &&
echo "Generating image remotely" &&
ssh fbd -C "cd qemu; sudo ./update_img.sh" &&
echo "Copying image back" &&
rsync fbd:~/qemu/test.img ../qemu/test.img --progress &&
echo "Done"
