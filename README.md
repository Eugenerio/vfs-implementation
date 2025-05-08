# Virtual File System

A simple ext2-like file system implementation in C++.

## Features

- Create a virtual disk with specified size
- Create and remove directories
- Copy files between the virtual disk and system disk
- List directory contents with file sizes
- Create hard links to files or directories
- Remove files or links
- Append data to files
- Truncate files
- Display disk usage information

## Building the Project

```bash
mkdir build
cd build
cmake ..
make
```

## Usage

Run the program with the path to the virtual disk file:

```bash
./vfs disk.img
```

If the disk file doesn't exist, you will be prompted to create a new one.

## Available Commands

- `mkdir <path>` - Create a directory
- `rmdir <path>` - Remove a directory
- `copyto <virt_path> <sys_path>` - Copy a file from virtual disk to system
- `copyfrom <sys_path> <virt_path>` - Copy a file from system to virtual disk
- `ls <path>` - List directory contents
- `link <target> <link_path>` - Create a hard link
- `rm <path>` - Remove a file or link
- `append <path> <bytes>` - Add bytes to a file
- `truncate <path> <bytes>` - Truncate a file by bytes
- `usage` - Show disk usage
- `help` - Show help
- `exit` - Exit the program

## File System Structure

The file system is structured similarly to ext2:

- Superblock: Contains metadata about the file system
- Block bitmap: Tracks which blocks are in use
- Inode tables: Store metadata about files and directories
- Data blocks: Store file and directory contents

Files have direct block pointers and a single indirect block pointer for larger files. 