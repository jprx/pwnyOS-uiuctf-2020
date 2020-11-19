FROM ubuntu:20.04
LABEL maintainer="jprx"

# Don't worry about prompts from installers
ENV DEBIAN_FRONTEND=noninteractive

RUN dpkg --add-architecture i386
RUN apt-get -y update --fix-missing && apt-get -y upgrade && apt-get -y install grub-pc-bin man gdb vim git binutils bridge-utils gdisk xorriso build-essential python3 python3-pip

# All dependencies at the same time: 
#RUN apt-get -y update --fix-missing && apt-get -y upgrade && apt-get -y install build-essential \
#vim git binutils grub2-common:i386 grub-pc-bin man gdb python3 python3-pip radare2 gdisk xorriso qemu

# RUN apt-get -y upgrade

# RUN apt-get -y install build-essential

# RUN apt-get -y install vim git
# RUN apt-get -y install binutils

# #RUN apt-get -y install grub2-common:i386
# RUN apt-get -y install grub-pc-bin
# RUN apt-get -y install man
# RUN apt-get -y install gdb

# # Some reverse engineering tools that are useful:
# RUN apt-get -y install python3 python3-pip
# RUN apt-get -y install radare2

# RUN apt-get -y install gdisk
# RUN apt-get -y install xorriso

# RUN apt-get -y install qemu

CMD /bin/sh
