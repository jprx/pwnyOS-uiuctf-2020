# Creates the fs.img file
# First build everything in user_src
cd ../fs
cd user_src
make

# Move all installed programs to /bin
mv rash ../files/fs/bin/rash
# mv crash ../files/fs/bin/crash
# mv kernpoke ../files/fs/bin/kernpoke
# mv spam ../files/fs/bin/spam
mv lsproc ../files/fs/bin/lsproc
mv binexec ../files/fs/bin/binexec
# mv mali ../files/fs/bin/mali
# mv mmapper ../files/fs/bin/mmapper
mv crazy_caches ../files/fs/prot/crazy_caches
# mv freaky ../files/fs/bin/freaky
# mv solve ../files/fs/bin/solve
# mv getroot ../files/fs/bin/getroot
mv level1 ../files/fs/prot/intros/level1
mv level2 ../files/fs/prot/intros/level2
cd ..

# Build the filesystem image
./make_fs.py ./files/fs ./files/fs.img
