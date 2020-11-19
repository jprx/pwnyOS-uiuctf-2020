# Link the kernel with the rest of the filesystem defined in /fs and store into /fs/jprxOS.img
rm -f ../fs/files/boot/kernel
cp kernel ../fs/files/boot/
cd ../fs
./make_image.sh