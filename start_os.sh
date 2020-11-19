# Snapshot marks the drive as readonly

# Call this with ./start_os.sh debug
# to enable debug mode

QEMU_64BIT=1

# Is this debug mode?
if [ -z "$1" ]
then
    DEBUG_STRING=""
    ALLOW_ACCELERATION=1
    QEMU_64BIT=1
elif [[ $1 == "debug" ]]; then
	# If we are debugging, run Qemu without hw acceleration in 32-bit mode
    DEBUG_STRING="-s -S"
    ALLOW_ACCELERATION=0
    QEMU_64BIT=0
fi

# Get name of current kernel
unameVal="$(uname -s)"

# Linux uses KVM, Mac uses something called "HVF"
if [[ $ALLOW_ACCELERATION == 1 ]]; then
	if [[ $unameVal == *"Darwin"* ]]; then
	    echo "Using macOS HVF Acceleration!"
	    ACCELERATOR="-machine accel=hvf"
	elif [[ $unameVal == *"Linux"* ]]; then
	    echo "Using KVM Acceleration!"
	    ACCELERATOR="-enable-kvm -cpu host"
	else
	    # @TODO: Windows acceleration
	    echo "Hardware acceleration is not being used!"
	fi
else
	echo "Hardware acceleration is not being used!"
fi

if [[ $QEMU_64BIT == 1 ]]; then
    echo "Using 64 bit Qemu"
	QEMU_CMD="qemu-system-x86_64"
else
    echo "Using 32 bit Qemu"
	QEMU_CMD="qemu-system-i386"
fi

# With Acceleration:
sudo $QEMU_CMD $ACCELERATOR $DEBUG_STRING -snapshot -hda fs/jprxOS.img -monitor stdio -vga virtio

#Without KVM:
#sudo qemu-system-i386 -snapshot -hda fs/jprxOS.img -monitor stdio -vga virtio
