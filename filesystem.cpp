#include "filesystem.h"
#include <cstring>
#include <iostream>
#include <algorithm>

// Magic number for our file system
constexpr uint32_t FS_MAGIC = 0x4D534653; // "FSMS"

FileSystem::FileSystem(const std::string &path) : disk_path(path)
{
}

FileSystem::~FileSystem()
{
    if (disk_file.is_open())
    {
        disk_file.close();
    }
}

bool FileSystem::create_disk(size_t size)
{
    // Round size to block size
    size_t num_blocks = (size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    size_t actual_size = num_blocks * BLOCK_SIZE;

    // Calculate number of inodes (roughly 1 inode per 4 blocks)
    size_t inodes_count = num_blocks / 4;
    size_t inode_blocks = (inodes_count * INODE_SIZE + BLOCK_SIZE - 1) / BLOCK_SIZE;

    disk_file.open(disk_path, std::ios::out | std::ios::binary);
    if (!disk_file)
    {
        return false;
    }

    // Initialize disk with zeros
    char *empty_block = new char[BLOCK_SIZE]();
    for (size_t i = 0; i < num_blocks; i++)
    {
        disk_file.write(empty_block, BLOCK_SIZE);
    }
    delete[] empty_block;

    // Initialize superblock
    superblock.magic = FS_MAGIC;
    superblock.block_size = BLOCK_SIZE;
    superblock.blocks_count = num_blocks;
    superblock.free_blocks_count = num_blocks - 2 - inode_blocks; // Subtract superblock, bitmap, and inode blocks
    superblock.inodes_count = inodes_count;
    superblock.free_inodes_count = inodes_count - 1; // Reserve first inode for root directory
    superblock.first_data_block = 2 + inode_blocks;
    superblock.first_inode_block = 2; // Right after superblock and bitmap
    superblock.bitmap_block = 1;

    // Write superblock
    disk_file.seekp(0, std::ios::beg);
    disk_file.write(reinterpret_cast<char *>(&superblock), sizeof(Superblock));

    // Initialize block bitmap
    block_bitmap.resize(num_blocks, false);
    block_bitmap[0] = true; // Superblock
    block_bitmap[1] = true; // Bitmap block
    for (size_t i = 0; i < inode_blocks; i++)
    {
        block_bitmap[2 + i] = true; // Inode blocks
    }
    write_bitmap();

    // Initialize inode table blocks
    char zero_inode_block[BLOCK_SIZE] = {0};
    for (size_t i = 0; i < inode_blocks; ++i)
    {
        disk_file.seekp((superblock.first_inode_block + i) * BLOCK_SIZE, std::ios::beg);
        disk_file.write(zero_inode_block, BLOCK_SIZE);
    }

    // After writing all inode table blocks
    disk_file.flush();

    // Create root directory
    Inode root_inode;
    // Explicitly set the mode to DIRECTORY
    root_inode.mode = static_cast<uint32_t>(FileType::DIRECTORY);
    root_inode.links_count = 1;

    // Allocate a block for root directory
    uint32_t root_block = allocate_block();
    root_inode.blocks[0] = root_block;

    // Initial directory entries (. and ..)
    char dir_block[BLOCK_SIZE] = {0};
    DirEntry *entries = reinterpret_cast<DirEntry *>(dir_block);

    // First entry (.)
    entries[0].inode = 1; // Root inode is always 1
    entries[0].rec_len = sizeof(DirEntry);
    entries[0].name_len = 1;
    entries[0].file_type = static_cast<uint8_t>(FileType::DIRECTORY);
    strcpy(entries[0].name, ".");

    // Second entry (..)
    entries[1].inode = 1; // Parent of root is root
    entries[1].rec_len = sizeof(DirEntry);
    entries[1].name_len = 2;
    entries[1].file_type = static_cast<uint8_t>(FileType::DIRECTORY);
    strcpy(entries[1].name, "..");

    // Write directory entries
    write_block(root_block, dir_block);

    // Directly write the root inode to the inode table block
    uint32_t inode_block = superblock.first_inode_block;
    char block_data[BLOCK_SIZE] = {0};
    memcpy(block_data, &root_inode, sizeof(Inode));
    disk_file.seekp(inode_block * BLOCK_SIZE, std::ios::beg);
    disk_file.write(block_data, BLOCK_SIZE);
    disk_file.flush();

    disk_file.close();
    return true;
}

bool FileSystem::mount_disk()
{
    disk_file.open(disk_path, std::ios::in | std::ios::out | std::ios::binary);
    if (!disk_file)
    {
        return false;
    }

    if (!read_superblock())
    {
        disk_file.close();
        return false;
    }

    if (superblock.magic != FS_MAGIC)
    {
        disk_file.close();
        return false;
    }

    if (!read_bitmap())
    {
        disk_file.close();
        return false;
    }

    return true;
}

bool FileSystem::read_superblock()
{
    disk_file.seekg(0, std::ios::beg);
    disk_file.read(reinterpret_cast<char *>(&superblock), sizeof(Superblock));
    return disk_file.good();
}

bool FileSystem::write_superblock()
{
    disk_file.seekp(0, std::ios::beg);
    disk_file.write(reinterpret_cast<const char *>(&superblock), sizeof(Superblock));
    return disk_file.good();
}

bool FileSystem::read_block(uint32_t block_num, void *buffer)
{
    if (block_num >= superblock.blocks_count)
    {
        return false;
    }

    disk_file.seekg(block_num * BLOCK_SIZE, std::ios::beg);
    disk_file.read(static_cast<char *>(buffer), BLOCK_SIZE);
    return disk_file.good();
}

bool FileSystem::write_block(uint32_t block_num, const void *buffer)
{
    if (block_num >= superblock.blocks_count)
    {
        return false;
    }

    disk_file.seekp(block_num * BLOCK_SIZE, std::ios::beg);
    disk_file.write(static_cast<const char *>(buffer), BLOCK_SIZE);
    return disk_file.good();
}

bool FileSystem::read_bitmap()
{
    char *bitmap_data = new char[BLOCK_SIZE];
    if (!read_block(superblock.bitmap_block, bitmap_data))
    {
        delete[] bitmap_data;
        return false;
    }

    block_bitmap.resize(superblock.blocks_count);
    for (uint32_t i = 0; i < superblock.blocks_count; i++)
    {
        if (i / 8 < BLOCK_SIZE)
        {
            block_bitmap[i] = (bitmap_data[i / 8] & (1 << (i % 8))) != 0;
        }
    }

    delete[] bitmap_data;
    return true;
}

bool FileSystem::write_bitmap()
{
    char *bitmap_data = new char[BLOCK_SIZE]();

    for (uint32_t i = 0; i < superblock.blocks_count && i / 8 < BLOCK_SIZE; i++)
    {
        if (block_bitmap[i])
        {
            bitmap_data[i / 8] |= (1 << (i % 8));
        }
    }

    bool result = write_block(superblock.bitmap_block, bitmap_data);
    delete[] bitmap_data;
    return result;
}

uint32_t FileSystem::allocate_block()
{
    for (uint32_t i = 0; i < superblock.blocks_count; i++)
    {
        if (!block_bitmap[i])
        {
            block_bitmap[i] = true;
            superblock.free_blocks_count--;
            write_bitmap();
            write_superblock();
            return i;
        }
    }
    return 0; // No free blocks
}

void FileSystem::free_block(uint32_t block_num)
{
    if (block_num < superblock.blocks_count && block_bitmap[block_num])
    {
        block_bitmap[block_num] = false;
        superblock.free_blocks_count++;
        write_bitmap();
        write_superblock();
    }
}

bool FileSystem::read_inode(uint32_t inode_num, Inode &inode)
{
    if (inode_num == 0 || inode_num > superblock.inodes_count)
    {
        return false;
    }

    uint32_t inode_block = superblock.first_inode_block + (inode_num - 1) / INODES_PER_BLOCK;
    uint32_t inode_offset = (inode_num - 1) % INODES_PER_BLOCK;

    char block_data[BLOCK_SIZE];
    if (!read_block(inode_block, block_data))
    {
        memset(block_data, 0, BLOCK_SIZE);
    }

    memcpy(&inode, block_data + inode_offset * INODE_SIZE, INODE_SIZE);

    return true;
}

bool FileSystem::write_inode(uint32_t inode_num, const Inode &inode)
{
    if (inode_num == 0 || inode_num > superblock.inodes_count)
    {
        return false;
    }

    uint32_t inode_block = superblock.first_inode_block + (inode_num - 1) / INODES_PER_BLOCK;
    uint32_t inode_offset = (inode_num - 1) % INODES_PER_BLOCK;

    char block_data[BLOCK_SIZE];
    // Try to read the block; if it doesn't exist, zero it
    if (!read_block(inode_block, block_data))
    {
        memset(block_data, 0, BLOCK_SIZE);
    }

    memcpy(block_data + inode_offset * INODE_SIZE, &inode, sizeof(Inode));
    return write_block(inode_block, block_data);
}

uint32_t FileSystem::allocate_inode()
{
    // Start from 1 as inode 0 is invalid
    for (uint32_t i = 1; i <= superblock.inodes_count; i++)
    {
        Inode inode;
        if (read_inode(i, inode) && inode.links_count == 0)
        {
            superblock.free_inodes_count--;
            write_superblock();
            return i;
        }
    }
    return 0; // No free inodes
}

void FileSystem::free_inode(uint32_t inode_num)
{
    if (inode_num == 0 || inode_num > superblock.inodes_count)
    {
        return;
    }

    Inode inode;
    if (read_inode(inode_num, inode))
    {
        // Free data blocks
        for (uint32_t i = 0; i < DIRECT_BLOCKS; i++)
        {
            if (inode.blocks[i] != 0)
            {
                free_block(inode.blocks[i]);
                inode.blocks[i] = 0;
            }
        }

        // Handle indirect blocks if needed
        if (inode.blocks[DIRECT_BLOCKS] != 0)
        {
            uint32_t indirect_block = inode.blocks[DIRECT_BLOCKS];
            uint32_t indirect_pointers[BLOCK_SIZE / sizeof(uint32_t)];

            if (read_block(indirect_block, indirect_pointers))
            {
                for (uint32_t i = 0; i < BLOCK_SIZE / sizeof(uint32_t); i++)
                {
                    if (indirect_pointers[i] != 0)
                    {
                        free_block(indirect_pointers[i]);
                    }
                }
            }

            free_block(indirect_block);
            inode.blocks[DIRECT_BLOCKS] = 0;
        }

        inode.links_count = 0;
        inode.size = 0;
        inode.mode = 0;

        write_inode(inode_num, inode);
        superblock.free_inodes_count++;
        write_superblock();
    }
}

std::string FileSystem::get_absolute_path(const std::string &path)
{
    std::string abs_path = path;

    // Normalize path
    if (abs_path.empty() || abs_path[0] != '/')
    {
        abs_path = "/" + abs_path;
    }

    // Remove trailing slash if it's not the root
    if (abs_path.length() > 1 && abs_path.back() == '/')
    {
        abs_path.pop_back();
    }

    return abs_path;
}

uint32_t FileSystem::find_inode_by_path(const std::string &path)
{
    std::string abs_path = get_absolute_path(path);

    // Root directory is special case
    if (abs_path == "/")
    {
        return 1; // Root inode
    }

    // Start from root directory
    uint32_t current_inode = 1;

    // Split path into components
    std::vector<std::string> components;
    std::string component;
    for (size_t i = 1; i < abs_path.length(); i++)
    {
        if (abs_path[i] == '/')
        {
            if (!component.empty())
            {
                components.push_back(component);
                component.clear();
            }
        }
        else
        {
            component += abs_path[i];
        }
    }
    if (!component.empty())
    {
        components.push_back(component);
    }

    // Traverse the directory tree
    for (const auto &comp : components)
    {
        Inode inode;
        if (!read_inode(current_inode, inode))
        {
            return 0;
        }

        // Check if the current inode is a directory
        if (static_cast<FileType>(inode.mode) != FileType::DIRECTORY)
        {
            return 0;
        }

        // Look for the component in the directory
        bool found = false;
        for (uint32_t i = 0; i < DIRECT_BLOCKS && inode.blocks[i] != 0; i++)
        {
            char block_data[BLOCK_SIZE];
            if (!read_block(inode.blocks[i], block_data))
            {
                continue;
            }

            // Scan directory entries
            char *ptr = block_data;
            while (ptr < block_data + BLOCK_SIZE)
            {
                DirEntry *entry = reinterpret_cast<DirEntry *>(ptr);
                if (entry->inode == 0 || entry->rec_len == 0)
                {
                    break;
                }

                std::string entry_name(entry->name, entry->name_len);

                if (strncmp(entry->name, comp.c_str(), entry->name_len) == 0 &&
                    comp.length() == entry->name_len)
                {
                    current_inode = entry->inode;
                    found = true;
                    break;
                }

                ptr += entry->rec_len;
            }

            if (found)
            {
                break;
            }
        }

        if (!found)
        {
            return 0; // Component not found
        }
    }

    return current_inode;
}

uint32_t FileSystem::create_file(const std::string &parent_path, const std::string &name, FileType type)
{
    // Find parent directory
    uint32_t parent_inode_num = find_inode_by_path(parent_path);

    if (parent_inode_num == 0)
    {
        return 0;
    }

    Inode parent_inode;
    if (!read_inode(parent_inode_num, parent_inode))
    {
        return 0;
    }

    // Check if parent is a directory
    if (static_cast<FileType>(parent_inode.mode) != FileType::DIRECTORY)
    {
        return 0;
    }

    // Check if file already exists
    for (uint32_t i = 0; i < DIRECT_BLOCKS && parent_inode.blocks[i] != 0; i++)
    {
        char block_data[BLOCK_SIZE];
        if (!read_block(parent_inode.blocks[i], block_data))
        {
            continue;
        }

        // Scan directory entries
        char *ptr = block_data;
        while (ptr < block_data + BLOCK_SIZE)
        {
            DirEntry *entry = reinterpret_cast<DirEntry *>(ptr);
            if (entry->inode == 0 || entry->rec_len == 0)
            {
                break;
            }

            if (strncmp(entry->name, name.c_str(), entry->name_len) == 0 &&
                name.length() == entry->name_len)
            {
                return 0; // File already exists
            }

            ptr += entry->rec_len;
        }
    }

    // Allocate new inode
    uint32_t new_inode_num = allocate_inode();
    if (new_inode_num == 0)
    {
        return 0;
    }

    // Initialize new inode
    Inode new_inode;
    new_inode.mode = static_cast<uint32_t>(type);
    new_inode.links_count = 1;

    if (type == FileType::DIRECTORY)
    {
        // Allocate a block for directory
        uint32_t dir_block = allocate_block();
        if (dir_block == 0)
        {
            free_inode(new_inode_num);
            return 0;
        }

        new_inode.blocks[0] = dir_block;

        // Set up directory entries (. and ..)
        DirEntry entries[2];
        entries[0].inode = new_inode_num;
        entries[0].rec_len = 12;
        entries[0].name_len = 1;
        entries[0].file_type = static_cast<uint8_t>(FileType::DIRECTORY);
        strcpy(entries[0].name, ".");

        entries[1].inode = parent_inode_num;
        entries[1].rec_len = 12;
        entries[1].name_len = 2;
        entries[1].file_type = static_cast<uint8_t>(FileType::DIRECTORY);
        strcpy(entries[1].name, "..");

        // Write directory entries
        write_block(dir_block, entries);
    }

    // Write new inode
    write_inode(new_inode_num, new_inode);

    // Add entry to parent directory
    bool entry_added = false;
    for (uint32_t i = 0; i < DIRECT_BLOCKS; i++)
    {
        if (parent_inode.blocks[i] == 0)
        {
            // Allocate new block for directory
            uint32_t new_block = allocate_block();
            if (new_block == 0)
            {
                free_inode(new_inode_num);
                return 0;
            }

            parent_inode.blocks[i] = new_block;

            // Initialize new directory block
            DirEntry new_entry;
            new_entry.inode = new_inode_num;
            new_entry.rec_len = sizeof(DirEntry);
            new_entry.name_len = name.length();
            new_entry.file_type = static_cast<uint8_t>(type);
            strncpy(new_entry.name, name.c_str(), 255);
            new_entry.name[255] = '\0';

            // Write directory block
            write_block(new_block, &new_entry);
            entry_added = true;
            break;
        }
        else
        {
            // Try to add to existing block
            char block_data[BLOCK_SIZE];
            if (read_block(parent_inode.blocks[i], block_data))
            {
                // Find space for new entry
                char *ptr = block_data;
                DirEntry *last_entry = nullptr;

                while (ptr < block_data + BLOCK_SIZE)
                {
                    DirEntry *entry = reinterpret_cast<DirEntry *>(ptr);
                    if (entry->inode == 0 || entry->rec_len == 0)
                    {
                        // Found empty slot
                        entry->inode = new_inode_num;
                        entry->rec_len = sizeof(DirEntry);
                        entry->name_len = name.length();
                        entry->file_type = static_cast<uint8_t>(type);
                        strncpy(entry->name, name.c_str(), 255);
                        entry->name[255] = '\0';

                        write_block(parent_inode.blocks[i], block_data);
                        entry_added = true;
                        break;
                    }

                    last_entry = entry;
                    ptr += entry->rec_len;
                }

                if (!entry_added && last_entry && ptr + sizeof(DirEntry) <= block_data + BLOCK_SIZE)
                {
                    // Add at the end
                    DirEntry *new_entry = reinterpret_cast<DirEntry *>(ptr);
                    new_entry->inode = new_inode_num;
                    new_entry->rec_len = sizeof(DirEntry);
                    new_entry->name_len = name.length();
                    new_entry->file_type = static_cast<uint8_t>(type);
                    strncpy(new_entry->name, name.c_str(), 255);
                    new_entry->name[255] = '\0';

                    write_block(parent_inode.blocks[i], block_data);
                    entry_added = true;
                }
            }
        }

        if (entry_added)
        {
            break;
        }
    }

    if (!entry_added)
    {
        free_inode(new_inode_num);
        return 0;
    }

    // Update parent inode
    write_inode(parent_inode_num, parent_inode);

    return new_inode_num;
}

bool FileSystem::create_directory(const std::string &path)
{
    // Get the parent path and name of the directory
    std::string abs_path = get_absolute_path(path);
    size_t pos = abs_path.find_last_of('/');
    std::string parent_path, name;

    if (pos == 0)
    {
        parent_path = "/";
        name = abs_path.substr(1);
    }
    else
    {
        parent_path = abs_path.substr(0, pos);
        name = abs_path.substr(pos + 1);
    }

    uint32_t inode_num = create_file(parent_path, name, FileType::DIRECTORY);
    return inode_num != 0;
}

bool FileSystem::remove_directory(const std::string &path)
{
    uint32_t dir_inode_num = find_inode_by_path(path);
    if (dir_inode_num == 0)
    {
        return false;
    }

    // Read directory inode
    Inode dir_inode;
    if (!read_inode(dir_inode_num, dir_inode))
    {
        return false;
    }

    // Check if it's a directory
    if (static_cast<FileType>(dir_inode.mode) != FileType::DIRECTORY)
    {
        return false;
    }

    // Check if directory is empty (except for . and ..)
    for (uint32_t i = 0; i < DIRECT_BLOCKS && dir_inode.blocks[i] != 0; i++)
    {
        char block_data[BLOCK_SIZE];
        if (!read_block(dir_inode.blocks[i], block_data))
        {
            continue;
        }

        // Scan directory entries
        char *ptr = block_data;
        while (ptr < block_data + BLOCK_SIZE)
        {
            DirEntry *entry = reinterpret_cast<DirEntry *>(ptr);
            if (entry->inode == 0 || entry->rec_len == 0)
            {
                break;
            }

            // Skip . and ..
            if (!(entry->name_len == 1 && entry->name[0] == '.') &&
                !(entry->name_len == 2 && entry->name[0] == '.' && entry->name[1] == '.'))
            {
                return false; // Directory not empty
            }

            ptr += entry->rec_len;
        }
    }

    // Get parent directory
    std::string abs_path = get_absolute_path(path);
    size_t pos = abs_path.find_last_of('/');
    std::string parent_path, name;

    if (pos == 0)
    {
        parent_path = "/";
        name = abs_path.substr(1);
    }
    else
    {
        parent_path = abs_path.substr(0, pos);
        name = abs_path.substr(pos + 1);
    }

    uint32_t parent_inode_num = find_inode_by_path(parent_path);
    if (parent_inode_num == 0)
    {
        return false;
    }

    Inode parent_inode;
    if (!read_inode(parent_inode_num, parent_inode))
    {
        return false;
    }

    // Remove directory entry from parent
    for (uint32_t i = 0; i < DIRECT_BLOCKS && parent_inode.blocks[i] != 0; i++)
    {
        char block_data[BLOCK_SIZE];
        if (!read_block(parent_inode.blocks[i], block_data))
        {
            continue;
        }

        // Scan directory entries
        char *ptr = block_data;
        while (ptr < block_data + BLOCK_SIZE)
        {
            DirEntry *entry = reinterpret_cast<DirEntry *>(ptr);
            if (entry->inode == 0 || entry->rec_len == 0)
            {
                break;
            }

            if (entry->inode == dir_inode_num)
            {
                // Remove entry
                entry->inode = 0;
                write_block(parent_inode.blocks[i], block_data);
                break;
            }

            ptr += entry->rec_len;
        }
    }

    // Free the directory's inode and blocks
    free_inode(dir_inode_num);

    return true;
}

bool FileSystem::copy_to_system(const std::string &virt_path, const std::string &sys_path)
{
    uint32_t file_inode_num = find_inode_by_path(virt_path);
    if (file_inode_num == 0)
    {
        return false;
    }

    Inode file_inode;
    if (!read_inode(file_inode_num, file_inode))
    {
        return false;
    }

    // Check if it's a regular file
    if (static_cast<FileType>(file_inode.mode) != FileType::REGULAR)
    {
        return false;
    }

    // Open system file for writing
    std::ofstream sys_file(sys_path, std::ios::binary);
    if (!sys_file)
    {
        return false;
    }

    // Copy data blocks
    uint32_t remaining_size = file_inode.size;

    // Direct blocks
    for (uint32_t i = 0; i < DIRECT_BLOCKS && remaining_size > 0 && file_inode.blocks[i] != 0; i++)
    {
        char block_data[BLOCK_SIZE];
        if (!read_block(file_inode.blocks[i], block_data))
        {
            sys_file.close();
            return false;
        }

        uint32_t write_size = std::min(remaining_size, static_cast<uint32_t>(BLOCK_SIZE));
        sys_file.write(block_data, write_size);
        remaining_size -= write_size;
    }

    // Handle indirect blocks if needed
    if (remaining_size > 0 && file_inode.blocks[DIRECT_BLOCKS] != 0)
    {
        uint32_t indirect_pointers[BLOCK_SIZE / sizeof(uint32_t)];
        if (!read_block(file_inode.blocks[DIRECT_BLOCKS], indirect_pointers))
        {
            sys_file.close();
            return false;
        }

        for (uint32_t i = 0; i < BLOCK_SIZE / sizeof(uint32_t) && remaining_size > 0; i++)
        {
            if (indirect_pointers[i] == 0)
            {
                continue;
            }

            char block_data[BLOCK_SIZE];
            if (!read_block(indirect_pointers[i], block_data))
            {
                sys_file.close();
                return false;
            }

            uint32_t write_size = std::min(remaining_size, static_cast<uint32_t>(BLOCK_SIZE));
            sys_file.write(block_data, write_size);
            remaining_size -= write_size;
        }
    }

    sys_file.close();
    return true;
}

bool FileSystem::copy_from_system(const std::string &sys_path, const std::string &virt_path)
{
    // Open system file for reading
    std::ifstream sys_file(sys_path, std::ios::binary | std::ios::ate);
    if (!sys_file)
    {
        return false;
    }

    // Get file size
    size_t file_size = sys_file.tellg();
    sys_file.seekg(0, std::ios::beg);

    // Create virtual file
    std::string abs_path = get_absolute_path(virt_path);
    size_t pos = abs_path.find_last_of('/');
    std::string parent_path, name;

    if (pos == 0)
    {
        parent_path = "/";
        name = abs_path.substr(1);
    }
    else
    {
        parent_path = abs_path.substr(0, pos);
        name = abs_path.substr(pos + 1);
    }

    uint32_t file_inode_num = create_file(parent_path, name, FileType::REGULAR);
    if (file_inode_num == 0)
    {
        return false;
    }

    Inode file_inode;
    if (!read_inode(file_inode_num, file_inode))
    {
        return false;
    }

    // Copy data blocks
    uint32_t remaining_size = file_size;
    uint32_t block_count = 0;

    // Direct blocks
    for (uint32_t i = 0; i < DIRECT_BLOCKS && remaining_size > 0; i++)
    {
        char block_data[BLOCK_SIZE] = {0};

        uint32_t read_size = std::min(remaining_size, static_cast<uint32_t>(BLOCK_SIZE));
        sys_file.read(block_data, read_size);

        uint32_t block_num = allocate_block();
        if (block_num == 0)
        {
            // Failed to allocate block, clean up
            file_inode.size = 0;
            write_inode(file_inode_num, file_inode);
            free_inode(file_inode_num);
            return false;
        }

        file_inode.blocks[i] = block_num;
        if (!write_block(block_num, block_data))
        {
            free_block(block_num);
            file_inode.blocks[i] = 0;
            file_inode.size = 0;
            write_inode(file_inode_num, file_inode);
            free_inode(file_inode_num);
            return false;
        }

        remaining_size -= read_size;
        block_count++;
    }

    // Handle indirect blocks if needed
    if (remaining_size > 0)
    {
        uint32_t indirect_block = allocate_block();
        if (indirect_block == 0)
        {
            // Clean up
            for (uint32_t i = 0; i < block_count; i++)
            {
                free_block(file_inode.blocks[i]);
                file_inode.blocks[i] = 0;
            }
            file_inode.size = 0;
            write_inode(file_inode_num, file_inode);
            free_inode(file_inode_num);
            return false;
        }

        file_inode.blocks[DIRECT_BLOCKS] = indirect_block;

        uint32_t indirect_pointers[BLOCK_SIZE / sizeof(uint32_t)] = {0};
        uint32_t indirect_count = 0;

        for (uint32_t i = 0; i < BLOCK_SIZE / sizeof(uint32_t) && remaining_size > 0; i++)
        {
            char block_data[BLOCK_SIZE] = {0};

            uint32_t read_size = std::min(remaining_size, static_cast<uint32_t>(BLOCK_SIZE));
            sys_file.read(block_data, read_size);

            uint32_t block_num = allocate_block();
            if (block_num == 0)
            {
                // Clean up
                for (uint32_t j = 0; j < block_count; j++)
                {
                    free_block(file_inode.blocks[j]);
                    file_inode.blocks[j] = 0;
                }
                for (uint32_t j = 0; j < indirect_count; j++)
                {
                    free_block(indirect_pointers[j]);
                }
                free_block(indirect_block);
                file_inode.blocks[DIRECT_BLOCKS] = 0;
                file_inode.size = 0;
                write_inode(file_inode_num, file_inode);
                free_inode(file_inode_num);
                return false;
            }

            indirect_pointers[i] = block_num;
            if (!write_block(block_num, block_data))
            {
                free_block(block_num);
                // Clean up
                for (uint32_t j = 0; j < block_count; j++)
                {
                    free_block(file_inode.blocks[j]);
                    file_inode.blocks[j] = 0;
                }
                for (uint32_t j = 0; j < indirect_count; j++)
                {
                    free_block(indirect_pointers[j]);
                }
                free_block(indirect_block);
                file_inode.blocks[DIRECT_BLOCKS] = 0;
                file_inode.size = 0;
                write_inode(file_inode_num, file_inode);
                free_inode(file_inode_num);
                return false;
            }

            remaining_size -= read_size;
            indirect_count++;
        }

        // Write indirect block pointers
        write_block(indirect_block, indirect_pointers);
    }

    // Update file size
    file_inode.size = file_size;
    write_inode(file_inode_num, file_inode);

    sys_file.close();
    return true;
}

std::vector<std::pair<std::string, uint32_t>> FileSystem::list_directory(const std::string &path)
{
    std::vector<std::pair<std::string, uint32_t>> result;

    uint32_t dir_inode_num = find_inode_by_path(path);
    if (dir_inode_num == 0)
    {
        return result;
    }

    Inode dir_inode;
    if (!read_inode(dir_inode_num, dir_inode))
    {
        return result;
    }

    // Check if it's a directory
    if (static_cast<FileType>(dir_inode.mode) != FileType::DIRECTORY)
    {
        return result;
    }

    // Scan directory blocks
    for (uint32_t i = 0; i < DIRECT_BLOCKS && dir_inode.blocks[i] != 0; i++)
    {
        char block_data[BLOCK_SIZE];
        if (!read_block(dir_inode.blocks[i], block_data))
        {
            continue;
        }

        // Scan directory entries
        char *ptr = block_data;
        while (ptr < block_data + BLOCK_SIZE)
        {
            DirEntry *entry = reinterpret_cast<DirEntry *>(ptr);
            if (entry->inode == 0 || entry->rec_len == 0)
            {
                break;
            }

            // Skip . and ..
            if ((entry->name_len == 1 && entry->name[0] == '.') ||
                (entry->name_len == 2 && entry->name[0] == '.' && entry->name[1] == '.'))
            {
                ptr += entry->rec_len;
                continue;
            }

            std::string name(entry->name, entry->name_len);

            // Read file/directory inode to get size
            Inode entry_inode;
            if (read_inode(entry->inode, entry_inode))
            {
                result.push_back(std::make_pair(name, entry_inode.size));
            }

            ptr += entry->rec_len;
        }
    }

    return result;
}

bool FileSystem::create_link(const std::string &target, const std::string &link_path)
{
    uint32_t target_inode_num = find_inode_by_path(target);
    if (target_inode_num == 0)
    {
        return false;
    }

    Inode target_inode;
    if (!read_inode(target_inode_num, target_inode))
    {
        return false;
    }

    // Get parent path and name for the link
    std::string abs_link_path = get_absolute_path(link_path);
    size_t pos = abs_link_path.find_last_of('/');
    std::string parent_path, name;

    if (pos == 0)
    {
        parent_path = "/";
        name = abs_link_path.substr(1);
    }
    else
    {
        parent_path = abs_link_path.substr(0, pos);
        name = abs_link_path.substr(pos + 1);
    }

    uint32_t parent_inode_num = find_inode_by_path(parent_path);
    if (parent_inode_num == 0)
    {
        return false;
    }

    Inode parent_inode;
    if (!read_inode(parent_inode_num, parent_inode))
    {
        return false;
    }

    // Check if parent is a directory
    if (static_cast<FileType>(parent_inode.mode) != FileType::DIRECTORY)
    {
        return false;
    }

    // Add directory entry for the link
    for (uint32_t i = 0; i < DIRECT_BLOCKS; i++)
    {
        if (parent_inode.blocks[i] == 0)
        {
            // Allocate new block for directory
            uint32_t new_block = allocate_block();
            if (new_block == 0)
            {
                return false;
            }

            parent_inode.blocks[i] = new_block;

            // Initialize new directory block
            DirEntry new_entry;
            new_entry.inode = target_inode_num;
            new_entry.rec_len = sizeof(DirEntry);
            new_entry.name_len = name.length();
            new_entry.file_type = static_cast<uint8_t>(static_cast<FileType>(target_inode.mode));
            strncpy(new_entry.name, name.c_str(), 255);
            new_entry.name[255] = '\0';

            // Write directory block
            write_block(new_block, &new_entry);
            break;
        }
        else
        {
            // Try to add to existing block
            char block_data[BLOCK_SIZE];
            if (read_block(parent_inode.blocks[i], block_data))
            {
                // Find space for new entry
                char *ptr = block_data;
                DirEntry *last_entry = nullptr;

                while (ptr < block_data + BLOCK_SIZE)
                {
                    DirEntry *entry = reinterpret_cast<DirEntry *>(ptr);
                    if (entry->inode == 0 || entry->rec_len == 0)
                    {
                        // Found empty slot
                        entry->inode = target_inode_num;
                        entry->rec_len = sizeof(DirEntry);
                        entry->name_len = name.length();
                        entry->file_type = static_cast<uint8_t>(static_cast<FileType>(target_inode.mode));
                        strncpy(entry->name, name.c_str(), 255);
                        entry->name[255] = '\0';

                        write_block(parent_inode.blocks[i], block_data);
                        break;
                    }

                    last_entry = entry;
                    ptr += entry->rec_len;
                }

                if (last_entry && ptr + sizeof(DirEntry) <= block_data + BLOCK_SIZE)
                {
                    // Add at the end
                    DirEntry *new_entry = reinterpret_cast<DirEntry *>(ptr);
                    new_entry->inode = target_inode_num;
                    new_entry->rec_len = sizeof(DirEntry);
                    new_entry->name_len = name.length();
                    new_entry->file_type = static_cast<uint8_t>(static_cast<FileType>(target_inode.mode));
                    strncpy(new_entry->name, name.c_str(), 255);
                    new_entry->name[255] = '\0';

                    write_block(parent_inode.blocks[i], block_data);
                    break;
                }
            }
        }
    }

    // Increment link count
    target_inode.links_count++;
    write_inode(target_inode_num, target_inode);

    // Update parent inode
    write_inode(parent_inode_num, parent_inode);

    return true;
}

bool FileSystem::remove_file(const std::string &path)
{
    uint32_t file_inode_num = find_inode_by_path(path);
    if (file_inode_num == 0)
    {
        return false;
    }

    Inode file_inode;
    if (!read_inode(file_inode_num, file_inode))
    {
        return false;
    }

    // Get parent directory
    std::string abs_path = get_absolute_path(path);
    size_t pos = abs_path.find_last_of('/');
    std::string parent_path, name;

    if (pos == 0)
    {
        parent_path = "/";
        name = abs_path.substr(1);
    }
    else
    {
        parent_path = abs_path.substr(0, pos);
        name = abs_path.substr(pos + 1);
    }

    uint32_t parent_inode_num = find_inode_by_path(parent_path);
    if (parent_inode_num == 0)
    {
        return false;
    }

    Inode parent_inode;
    if (!read_inode(parent_inode_num, parent_inode))
    {
        return false;
    }

    // Remove directory entry from parent
    for (uint32_t i = 0; i < DIRECT_BLOCKS && parent_inode.blocks[i] != 0; i++)
    {
        char block_data[BLOCK_SIZE];
        if (!read_block(parent_inode.blocks[i], block_data))
        {
            continue;
        }

        // Scan directory entries
        char *ptr = block_data;
        while (ptr < block_data + BLOCK_SIZE)
        {
            DirEntry *entry = reinterpret_cast<DirEntry *>(ptr);
            if (entry->inode == 0 || entry->rec_len == 0)
            {
                break;
            }

            if (entry->inode == file_inode_num)
            {
                // Remove entry
                entry->inode = 0;
                write_block(parent_inode.blocks[i], block_data);
                break;
            }

            ptr += entry->rec_len;
        }
    }

    // Decrement link count
    file_inode.links_count--;

    if (file_inode.links_count == 0)
    {
        // Free the file's blocks and inode
        free_inode(file_inode_num);
    }
    else
    {
        // Just update the inode
        write_inode(file_inode_num, file_inode);
    }

    return true;
}

bool FileSystem::append_to_file(const std::string &path, size_t bytes)
{
    uint32_t file_inode_num = find_inode_by_path(path);
    if (file_inode_num == 0)
    {
        return false;
    }

    Inode file_inode;
    if (!read_inode(file_inode_num, file_inode))
    {
        return false;
    }

    // Check if it's a regular file
    if (static_cast<FileType>(file_inode.mode) != FileType::REGULAR)
    {
        return false;
    }

    // Calculate current block count and position
    uint32_t current_size = file_inode.size;
    uint32_t current_blocks = (current_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    uint32_t position_in_last_block = current_size % BLOCK_SIZE;

    // Generate random data to append
    char *append_data = new char[bytes];
    for (size_t i = 0; i < bytes; i++)
    {
        append_data[i] = 'A' + (i % 26);
    }

    // Append data
    uint32_t bytes_left = bytes;
    uint32_t data_offset = 0;

    // First, fill the last block if it's not full
    if (position_in_last_block > 0 && current_blocks > 0)
    {
        uint32_t block_index = current_blocks - 1;
        uint32_t block_num;

        if (block_index < DIRECT_BLOCKS)
        {
            block_num = file_inode.blocks[block_index];
        }
        else
        {
            // Use indirect block
            uint32_t indirect_index = block_index - DIRECT_BLOCKS;
            uint32_t indirect_pointers[BLOCK_SIZE / sizeof(uint32_t)];

            if (!read_block(file_inode.blocks[DIRECT_BLOCKS], indirect_pointers))
            {
                delete[] append_data;
                return false;
            }

            block_num = indirect_pointers[indirect_index];
        }

        char block_data[BLOCK_SIZE];
        if (!read_block(block_num, block_data))
        {
            delete[] append_data;
            return false;
        }

        uint32_t space_left = BLOCK_SIZE - position_in_last_block;
        uint32_t bytes_to_write = std::min(bytes_left, space_left);

        memcpy(block_data + position_in_last_block, append_data, bytes_to_write);

        if (!write_block(block_num, block_data))
        {
            delete[] append_data;
            return false;
        }

        bytes_left -= bytes_to_write;
        data_offset += bytes_to_write;
    }

    // Allocate new blocks for remaining data
    while (bytes_left > 0)
    {
        uint32_t new_block = allocate_block();
        if (new_block == 0)
        {
            delete[] append_data;
            return false;
        }

        char block_data[BLOCK_SIZE] = {0};
        uint32_t bytes_to_write = std::min(bytes_left, static_cast<uint32_t>(BLOCK_SIZE));

        memcpy(block_data, append_data + data_offset, bytes_to_write);

        if (!write_block(new_block, block_data))
        {
            free_block(new_block);
            delete[] append_data;
            return false;
        }

        // Store block pointer
        if (current_blocks < DIRECT_BLOCKS)
        {
            file_inode.blocks[current_blocks] = new_block;
        }
        else
        {
            // Use indirect block
            uint32_t indirect_index = current_blocks - DIRECT_BLOCKS;

            if (indirect_index == 0)
            {
                // Allocate indirect block
                uint32_t indirect_block = allocate_block();
                if (indirect_block == 0)
                {
                    free_block(new_block);
                    delete[] append_data;
                    return false;
                }

                file_inode.blocks[DIRECT_BLOCKS] = indirect_block;

                uint32_t indirect_pointers[BLOCK_SIZE / sizeof(uint32_t)] = {0};
                indirect_pointers[0] = new_block;

                if (!write_block(indirect_block, indirect_pointers))
                {
                    free_block(indirect_block);
                    free_block(new_block);
                    file_inode.blocks[DIRECT_BLOCKS] = 0;
                    delete[] append_data;
                    return false;
                }
            }
            else
            {
                // Update existing indirect block
                uint32_t indirect_pointers[BLOCK_SIZE / sizeof(uint32_t)];

                if (!read_block(file_inode.blocks[DIRECT_BLOCKS], indirect_pointers))
                {
                    free_block(new_block);
                    delete[] append_data;
                    return false;
                }

                indirect_pointers[indirect_index] = new_block;

                if (!write_block(file_inode.blocks[DIRECT_BLOCKS], indirect_pointers))
                {
                    free_block(new_block);
                    delete[] append_data;
                    return false;
                }
            }
        }

        bytes_left -= bytes_to_write;
        data_offset += bytes_to_write;
        current_blocks++;
    }

    // Update file size
    file_inode.size += bytes;
    write_inode(file_inode_num, file_inode);

    delete[] append_data;
    return true;
}

bool FileSystem::truncate_file(const std::string &path, size_t bytes)
{
    uint32_t file_inode_num = find_inode_by_path(path);
    if (file_inode_num == 0)
    {
        return false;
    }

    Inode file_inode;
    if (!read_inode(file_inode_num, file_inode))
    {
        return false;
    }

    // Check if it's a regular file
    if (static_cast<FileType>(file_inode.mode) != FileType::REGULAR)
    {
        return false;
    }

    // Check if file is big enough to truncate
    if (file_inode.size < bytes)
    {
        return false;
    }

    // Calculate new size and blocks needed
    uint32_t new_size = file_inode.size - bytes;
    uint32_t new_blocks = (new_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    uint32_t current_blocks = (file_inode.size + BLOCK_SIZE - 1) / BLOCK_SIZE;

    // Free blocks that are no longer needed
    if (new_blocks < current_blocks)
    {
        // First, handle indirect blocks if any
        if (current_blocks > DIRECT_BLOCKS && file_inode.blocks[DIRECT_BLOCKS] != 0)
        {
            uint32_t indirect_pointers[BLOCK_SIZE / sizeof(uint32_t)];

            if (read_block(file_inode.blocks[DIRECT_BLOCKS], indirect_pointers))
            {
                for (uint32_t i = (new_blocks > DIRECT_BLOCKS ? new_blocks - DIRECT_BLOCKS : 0);
                     i < current_blocks - DIRECT_BLOCKS; i++)
                {
                    if (indirect_pointers[i] != 0)
                    {
                        free_block(indirect_pointers[i]);
                        indirect_pointers[i] = 0;
                    }
                }

                // If we don't need the indirect block anymore, free it
                if (new_blocks <= DIRECT_BLOCKS)
                {
                    free_block(file_inode.blocks[DIRECT_BLOCKS]);
                    file_inode.blocks[DIRECT_BLOCKS] = 0;
                }
                else
                {
                    // Otherwise write back the updated pointers
                    write_block(file_inode.blocks[DIRECT_BLOCKS], indirect_pointers);
                }
            }
        }

        // Handle direct blocks
        for (uint32_t i = new_blocks; i < std::min(current_blocks, static_cast<uint32_t>(DIRECT_BLOCKS)); i++)
        {
            if (file_inode.blocks[i] != 0)
            {
                free_block(file_inode.blocks[i]);
                file_inode.blocks[i] = 0;
            }
        }
    }

    // Update file size
    file_inode.size = new_size;
    write_inode(file_inode_num, file_inode);

    return true;
}

std::pair<uint32_t, uint32_t> FileSystem::get_disk_usage()
{
    uint32_t used_blocks = superblock.blocks_count - superblock.free_blocks_count;
    uint32_t total_blocks = superblock.blocks_count;

    return std::make_pair(used_blocks, total_blocks);
}