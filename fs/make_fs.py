#!/usr/bin/python3
# Make a filesystem
# This filesystem is very non-standard
# There are many like it but this one is mine

# -----------------------
# Layout

# Blocks are 1 page each (4kB)
# Blocks can either be data or directory blocks
# inodes are referred to as "fentries" because the name inode sucks
# Data blocks are either fentry blocks or "pure data" blocks
# Fentry blocks might be able to have data stored in them too, not sure yet

# -----------------------
# Directory blocks

# First 2 longs are the flags vector
# Bytes 0-3: Magic number for this filesystem (0xdeadd150 for dir block, 0xdeadda7a for data)
# Bytes 4-7: Number of valid entries (whether they be dir or data)
# Then a 64-byte name buffer

# The rest is just dentries
# Each dentry is just a block index to either another directory block or a fentry block

# Directory blocks can have up to 1023 things in it (that's a lot!)

# -----------------------
# Fentry
# Each Fentry can be up to ~4 MB (just a little less than that)

# The first few bytes of a data block may be a fentry
# Otherwise, this data block is just a "pure data" block
# (IE blocks can start as a fentry and then include data)

# First 64 bytes are this file's name
# Next 4 bytes are the number of valid entries in this fentry
# The rest are pointers to data blocks

# -----------------------
# Data block
#
# First 4 bytes are the size
# The rest is:
# Its just some data (TM)
# -----------------------

import os
import sys
import math
import struct
import binascii

# Size of a block
BLOCK_SIZE = 4096 # 4096

# Magicnum for directory block
JPRX_FS_MAGICNUM_DIR = 0xdeadd150

# Magicnum for data block
JPRX_FS_MAGICNUM_DAT = 0xdeadda7a

# Max number of files in a directory:
MAX_FILES_PER_DIR = 1000

# Largest allowed file
MAX_FILESIZE = 1000 * BLOCK_SIZE

NAME_LEN = 64

DEBUG_MODE = False

# Global vars:
outfile = "./files/fs.img"
infile = "./files"

# The global blocks list:
blocks = []

# Directory mapping a directory name to index in blocks array:
dir_name_to_block_idx = {}

class DataBlock():
    # Whatever's in it
    def set_contents(self, content):
        self.contents = content

    def __init__(self):
        self.contents = 0

    def set_idx(self, my_index, parent_index):
        self.my_index = my_index
        self.fentry_index = parent_index

    def __str__(self):
        return "{} Data block for fentry {}".format(self.my_index, self.fentry_index)

    def calculate_bytes(self):
        self.bytes = bytearray()
        self.bytes += struct.pack("<I", len(self.contents))
        self.bytes += self.contents
        
        for x in range(BLOCK_SIZE - len(self.bytes)):
            self.bytes += b'\xff'

class Fentry():
    """It's like an Inode but with a better name"""
    # name = ""

    # List of DataBlock's
    # data_blocks = []

    # my_index = 0

    def __init__(self, name_in, sys_path):
        if (not os.path.isfile(sys_path)):
            print ("Error: {} is not a file".format(sys_path))
            exit(-1)
        self.name = name_in
        self.sys_path = sys_path
        self.my_index = -1
        self.data_blocks = []
        self.num_blocks = 0

    def create_data_blocks(self):
        file_size = os.path.getsize(self.sys_path)
        if (file_size > MAX_FILESIZE):
            print("File {} is too big!".format(self.sys_path))
            exit(-1)
        file = open(self.sys_path, "rb")

        self.num_blocks = math.ceil(file_size / BLOCK_SIZE)
        for i in range(self.num_blocks):
            data_block = DataBlock()
            data_block.set_contents(file.read(BLOCK_SIZE - 4))
            data_block.set_idx(len(blocks), self.my_index)
            blocks.append(data_block)
            self.data_blocks.append(data_block.my_index)
        file.close()

    def set_index(self, idx):
        self.my_index = idx

    def __str__(self):
        return "{} Fentry {}\n\tData blocks ({}): {}".format(self.my_index, self.name, len(self.data_blocks), self.data_blocks)

    def calculate_bytes(self):
        # Start with magic number
        self.bytes = bytearray()
        self.bytes += struct.pack("<I", JPRX_FS_MAGICNUM_DAT)
        self.bytes += struct.pack("<I", len(self.data_blocks))
        
        # Name:
        name_counter = 0
        if (len(self.name) >= NAME_LEN):
            print("File {} has name too big ({})".format(self.name, len(self.name)))
            exit(-1)

        for name_counter in range(NAME_LEN):
            if (name_counter < len(self.name)):
                self.bytes += self.name[name_counter].encode('ASCII')
            else:
                self.bytes += b'\x00'

        # Subdirs:
        for b in self.data_blocks:
            self.bytes += struct.pack("<I", b)

        # Fill in rest of data:
        for x in range(BLOCK_SIZE - len(self.bytes)):
            self.bytes += b'\xff'

class DirectoryBlock():
    name = ""

    # List of Fentry objects (as indices to blocks):
    # fentries = []

    # # List of subdirectory dentries (as indices to blocks):
    # dentries = []

    # List of subdirectory names:
    # subdir_names = []

    # # My index
    # my_index = 0

    def __init__(self, sys_path, name_in):
        self.sys_path = sys_path
        self.name = name_in
        self.fentries = []
        self.dentries = []
        self.subdir_names = []
        self.my_index = 0

    def add_fentry(self, fentry):
        self.fentries.append(fentry)

    def add_subdir(self, dentry):
        self.dentries.append(dentry)

    def set_index(self, idx):
        self.my_index = idx

    def __str__(self):
        return "{}: Directory {}\n\tSubdirs: {}\n\tFiles: {}".format(self.my_index, self.name, self.dentries, self.fentries)

    def calculate_bytes(self):
        # Don't forget x86 is little endian

        # Start with magic number
        self.bytes = bytearray()
        self.bytes += struct.pack("<I", JPRX_FS_MAGICNUM_DIR)
        self.bytes += struct.pack("<I", len(self.dentries) + len(self.fentries))
        
        # Name:
        name_counter = 0
        if (len(self.name) >= NAME_LEN):
            print("Directory {} has name too big ({})".format(self.name, len(self.name)))
            exit(-1)

        for name_counter in range(NAME_LEN):
            if (name_counter < len(self.name)):
                self.bytes += self.name[name_counter].encode('ASCII')
            else:
                self.bytes += b'\x00'

        # Subdirs:
        for dentry in self.dentries:
            self.bytes += struct.pack("<I", dentry)

        # Fentry pointers:
        for fentry in self.fentries:
            self.bytes += struct.pack("<I", fentry)

        # Fill in rest of data:
        for x in range(BLOCK_SIZE - len(self.bytes)):
            self.bytes += b'\xff'

def check_args():
    global outfile
    global infile
    outfile = "./files/fs.img"
    if (len(sys.argv) != 2 and len(sys.argv) != 3):
        print("Usage: ./make_fs.py input_dir output_name")
        exit()

    if (len(sys.argv) == 2):
        if (DEBUG_MODE):
            print("Setting output file to be {}".format(outfile))
    else:
        outfile = sys.argv[2]

    #if (os.path.exists(outfile)):
        #print("Error: file {} already exists".format(outfile))
        #exit()

    infile = sys.argv[1]

    if (not os.path.isdir(infile)):
        print("Error: file {} isn't a directory".format(infile))
        exit()


# Actually generate the image:
def gen_blocks():
    # By changing directory to the one we want to traverse, we
    # make all paths automagically relative to '.':
    if (DEBUG_MODE):
        print(infile)
    orig_location = os.getcwd()
    os.chdir(infile)
    root = '.'

    for dir_name, subdir_list, file_list in os.walk(root):
        if (len(subdir_list) + len(file_list) > MAX_FILES_PER_DIR):
            print("Directory {} too big".format(dir_name))
            exit()

        if (DEBUG_MODE):
            print ("Found directory {}".format(dir_name))
        this_block = DirectoryBlock(dir_name, os.path.basename(dir_name))
        this_block.set_index(len(blocks))
        blocks.append(this_block)

        subdir_full_list = []
        for subdir in subdir_list:
            if (DEBUG_MODE):
                print ("Has subdirectory {}".format(subdir))
            subdir_full_list.append(os.path.join(dir_name,subdir))
        blocks[this_block.my_index].subdir_names = subdir_full_list

        for file in file_list:
            if (DEBUG_MODE):
                print ("And file {}".format(file))
            if (file == ".DS_Store"):
                print("Skipping .DS_Store")
            else:
                file_fentry = Fentry(file, os.path.join(dir_name, file))
                file_fentry.set_index(len(blocks))
                blocks.append(file_fentry)
                blocks[file_fentry.my_index].create_data_blocks()
                blocks[this_block.my_index].add_fentry(file_fentry.my_index)

        dir_name_to_block_idx[dir_name] = this_block.my_index

    # Now we need to iterate through blocks list
    # Every directory will have a directory block somewhere but they're not linked yet
    # For each block, if its a directory block, need to traverse all subdir_names
    # If its a fentry block, need to allocate the necessary data blocks
    for b in blocks:
        if (isinstance(b, DirectoryBlock)):
            for subdir_name in b.subdir_names:
                b.add_subdir(dir_name_to_block_idx[subdir_name])

    if (DEBUG_MODE):
        for b in blocks:
            print(b)

    os.chdir(orig_location)
    output_file = open(outfile, "wb")
    for b in blocks:
        b.calculate_bytes()
        if (len(b.bytes) != BLOCK_SIZE):
            print("Failure: block {} doesn't have correct number of bytes!".format(b.my_index))
            print("Got {} expected {} (= BLOCK_SIZE)".format(len(b.bytes), BLOCK_SIZE))
            exit(-1)
        output_file.write(b.bytes)

    if (DEBUG_MODE):
        print("Done!")

if __name__ == "__main__":
    check_args()
    gen_blocks()
