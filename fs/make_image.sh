# @TODO: Use grub-mkstandalone instead
#./make_fs.py ./files/fs ./files/fs.img
grub-mkrescue ./files -o jprxOS.img > /dev/null 2>&1
grub-mkrescue ./files -o jprxOS.iso > /dev/null 2>&1
