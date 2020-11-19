# See: https://wiki.archlinux.org/index.php/QEMU#VNC
# Uses environment variable VNC_PASSWORD and VNC_PORT
# Connect to this using open vnc://127.0.0.1:VNC_PORT on Mac
echo "Running with KVM Acceleration"
printf "change vnc password\n%s\n" $VNC_PASSWORD | qemu-system-i386 --enable-kvm -cpu host -snapshot -hda /fs/jprxOS.img -monitor stdio -vnc :$VNC_PORT,password
