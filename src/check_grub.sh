# See: https://wiki.osdev.org/Bare_Bones
if grub-file --is-x86-multiboot kernel; then
	echo "kernel is multiboot"
else
	echo "kernel is NOT multiboot"
fi
