#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <string>
#include <vector>
#include <cstdint>
#include <fstream>
#include <memory>
#include <cstring>

// Constants for file system structure
constexpr size_t BLOCK_SIZE = 4096; // 4KB blocks
constexpr size_t INODE_SIZE = 128;  // Size of inode in bytes
constexpr size_t INODES_PER_BLOCK = BLOCK_SIZE / INODE_SIZE;
constexpr size_t DIRECT_BLOCKS = 12;  // Direct block pointers in inode
constexpr size_t INDIRECT_BLOCKS = 1; // Single indirect block pointer

// File types
enum class FileType
{
    NONE = 0,
    REGULAR = 1,
    DIRECTORY = 2,
    SYMLINK = 3
};

// Superblock structure
struct Superblock
{
    uint32_t magic;             // Magic number to identify filesystem
    uint32_t block_size;        // Size of blocks in bytes
    uint32_t blocks_count;      // Total number of blocks
    uint32_t free_blocks_count; // Number of free blocks
    uint32_t inodes_count;      // Total number of inodes
    uint32_t free_inodes_count; // Number of free inodes
    uint32_t first_data_block;  // First data block
    uint32_t first_inode_block; // First inode block
    uint32_t bitmap_block;      // Block bitmap location
};

// Inode structure
#pragma pack(push, 1)
struct Inode
{
    uint32_t mode;                                    // 4
    uint32_t size;                                    // 4
    uint32_t links_count;                             // 4
    uint32_t blocks[DIRECT_BLOCKS + INDIRECT_BLOCKS]; // 13 * 4 = 52
    uint8_t reserved[128 - (4 + 4 + 4 + 52)];         // 68 bytes padding
    Inode()
    {
        mode = 0;
        size = 0;
        links_count = 0;
        for (size_t i = 0; i < DIRECT_BLOCKS + INDIRECT_BLOCKS; i++)
        {
            blocks[i] = 0;
        }
        memset(reserved, 0, sizeof(reserved));
    }
};
#pragma pack(pop)

// Directory entry structure
struct DirEntry
{
    uint32_t inode;    // Inode number
    uint16_t rec_len;  // Entry length
    uint8_t name_len;  // Name length
    uint8_t file_type; // File type
    char name[256];    // Filename

    DirEntry()
    {
        inode = 0;
        rec_len = 0;
        name_len = 0;
        file_type = 0;
        name[0] = '\0';
    }
};

// File system class
class FileSystem
{
private:
    std::string disk_path;
    std::fstream disk_file;
    Superblock superblock;
    std::vector<bool> block_bitmap;

    // Helper methods
    bool read_superblock();
    bool write_superblock();
    bool read_block(uint32_t block_num, void *buffer);
    bool write_block(uint32_t block_num, const void *buffer);
    bool read_inode(uint32_t inode_num, Inode &inode);
    bool write_inode(uint32_t inode_num, const Inode &inode);
    uint32_t allocate_block();
    void free_block(uint32_t block_num);
    uint32_t allocate_inode();
    void free_inode(uint32_t inode_num);
    bool read_bitmap();
    bool write_bitmap();
    std::string get_absolute_path(const std::string &path);
    uint32_t find_inode_by_path(const std::string &path);
    uint32_t create_file(const std::string &parent_path, const std::string &name, FileType type);

public:
    FileSystem(const std::string &disk_path);
    ~FileSystem();

    // Main operations
    bool create_disk(size_t size);
    bool mount_disk();
    bool create_directory(const std::string &path);
    bool remove_directory(const std::string &path);
    bool copy_to_system(const std::string &virt_path, const std::string &sys_path);
    bool copy_from_system(const std::string &sys_path, const std::string &virt_path);
    std::vector<std::pair<std::string, uint32_t>> list_directory(const std::string &path);
    bool create_link(const std::string &target, const std::string &link_path);
    bool remove_file(const std::string &path);
    bool append_to_file(const std::string &path, size_t bytes);
    bool truncate_file(const std::string &path, size_t bytes);
    std::pair<uint32_t, uint32_t> get_disk_usage(); // Returns <used, total> in blocks
};

#endif // FILESYSTEM_H