#include "filesystem.h"
#include <iostream>
#include <string>
#include <sstream>
#include <filesystem>
#include <iomanip>

// Helper function to print usage information
void print_usage() {
    std::cout << "Available commands:\n";
    std::cout << "  mkdir <path> - Create a directory\n";
    std::cout << "  rmdir <path> - Remove a directory\n";
    std::cout << "  copyto <virt_path> <sys_path> - Copy a file from virtual disk to system\n";
    std::cout << "  copyfrom <sys_path> <virt_path> - Copy a file from system to virtual disk\n";
    std::cout << "  ls <path> - List directory contents\n";
    std::cout << "  link <target> <link_path> - Create a hard link\n";
    std::cout << "  rm <path> - Remove a file or link\n";
    std::cout << "  append <path> <bytes> - Add bytes to a file\n";
    std::cout << "  truncate <path> <bytes> - Truncate a file by bytes\n";
    std::cout << "  usage - Show disk usage\n";
    std::cout << "  clear - Clear the screen\n";
    std::cout << "  help - Show this help\n";
    std::cout << "  exit - Exit the program\n";
}

// Helper function to parse and execute commands
bool execute_command(const std::string& input, FileSystem& fs) {
    std::istringstream iss(input);
    std::string cmd;
    
    iss >> cmd;
    
    if (cmd == "exit") {
        return false;
    } else if (cmd == "help") {
        print_usage();
    } else if (cmd == "clear") {
        // Clear screen using ANSI escape sequence (works in most terminals)
        std::cout << "\033[2J\033[1;1H";
    } else if (cmd == "mkdir") {
        std::string path;
        iss >> path;
        
        if (path.empty()) {
            std::cout << "Error: Missing path parameter\n";
            return true;
        }
        
        // Add debug info
        std::cout << "Debug: Trying to create directory '" << path << "'\n";
        
        if (fs.create_directory(path)) {
            std::cout << "Directory created successfully\n";
        } else {
            std::cout << "Error: Failed to create directory\n";
        }
    } else if (cmd == "rmdir") {
        std::string path;
        iss >> path;
        
        if (path.empty()) {
            std::cout << "Error: Missing path parameter\n";
            return true;
        }
        
        if (fs.remove_directory(path)) {
            std::cout << "Directory removed successfully\n";
        } else {
            std::cout << "Error: Failed to remove directory\n";
        }
    } else if (cmd == "copyto") {
        std::string virt_path, sys_path;
        iss >> virt_path >> sys_path;
        
        if (virt_path.empty() || sys_path.empty()) {
            std::cout << "Error: Missing parameters\n";
            return true;
        }
        
        if (fs.copy_to_system(virt_path, sys_path)) {
            std::cout << "File copied successfully\n";
        } else {
            std::cout << "Error: Failed to copy file\n";
        }
    } else if (cmd == "copyfrom") {
        std::string sys_path, virt_path;
        iss >> sys_path >> virt_path;
        
        if (sys_path.empty() || virt_path.empty()) {
            std::cout << "Error: Missing parameters\n";
            return true;
        }
        
        // Add debug info
        std::cout << "Debug: Trying to copy from '" << sys_path << "' to '" << virt_path << "'\n";
        
        if (!std::filesystem::exists(sys_path)) {
            std::cout << "Error: System file does not exist\n";
            return true;
        }
        
        if (fs.copy_from_system(sys_path, virt_path)) {
            std::cout << "File copied successfully\n";
        } else {
            std::cout << "Error: Failed to copy file\n";
        }
    } else if (cmd == "ls") {
        std::string path;
        iss >> path;
        
        if (path.empty()) {
            path = "/";
        }
        
        auto entries = fs.list_directory(path);
        
        if (entries.empty()) {
            std::cout << "Directory is empty or does not exist\n";
        } else {
            uint32_t total_size = 0;
            std::cout << "Contents of " << path << ":\n";
            std::cout << std::left << std::setw(30) << "Name" << std::right << std::setw(10) << "Size (B)" << "\n";
            std::cout << std::string(40, '-') << "\n";
            
            for (const auto& entry : entries) {
                std::cout << std::left << std::setw(30) << entry.first 
                          << std::right << std::setw(10) << entry.second << "\n";
                total_size += entry.second;
            }
            
            std::cout << std::string(40, '-') << "\n";
            std::cout << "Total size: " << total_size << " bytes\n";
        }
    } else if (cmd == "link") {
        std::string target, link_path;
        iss >> target >> link_path;
        
        if (target.empty() || link_path.empty()) {
            std::cout << "Error: Missing parameters\n";
            return true;
        }
        
        if (fs.create_link(target, link_path)) {
            std::cout << "Link created successfully\n";
        } else {
            std::cout << "Error: Failed to create link\n";
        }
    } else if (cmd == "rm") {
        std::string path;
        iss >> path;
        
        if (path.empty()) {
            std::cout << "Error: Missing path parameter\n";
            return true;
        }
        
        if (fs.remove_file(path)) {
            std::cout << "File removed successfully\n";
        } else {
            std::cout << "Error: Failed to remove file\n";
        }
    } else if (cmd == "append") {
        std::string path;
        size_t bytes;
        iss >> path >> bytes;
        
        if (path.empty() || bytes == 0) {
            std::cout << "Error: Missing or invalid parameters\n";
            return true;
        }
        
        if (fs.append_to_file(path, bytes)) {
            std::cout << bytes << " bytes appended successfully\n";
        } else {
            std::cout << "Error: Failed to append to file\n";
        }
    } else if (cmd == "truncate") {
        std::string path;
        size_t bytes;
        iss >> path >> bytes;
        
        if (path.empty() || bytes == 0) {
            std::cout << "Error: Missing or invalid parameters\n";
            return true;
        }
        
        if (fs.truncate_file(path, bytes)) {
            std::cout << "File truncated by " << bytes << " bytes successfully\n";
        } else {
            std::cout << "Error: Failed to truncate file\n";
        }
    } else if (cmd == "usage") {
        auto usage = fs.get_disk_usage();
        
        std::cout << "Disk usage:\n";
        std::cout << "Used: " << usage.first << " blocks (" 
                  << usage.first * BLOCK_SIZE << " bytes)\n";
        std::cout << "Total: " << usage.second << " blocks (" 
                  << usage.second * BLOCK_SIZE << " bytes)\n";
        std::cout << "Free: " << (usage.second - usage.first) << " blocks (" 
                  << (usage.second - usage.first) * BLOCK_SIZE << " bytes)\n";
        std::cout << "Usage: " << std::fixed << std::setprecision(2)
                  << (static_cast<double>(usage.first) / usage.second * 100) << "%\n";
    } else {
        std::cout << "Unknown command: " << cmd << "\n";
        print_usage();
    }
    
    return true;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <disk_file>\n";
        return 1;
    }
    
    std::string disk_path = argv[1];
    FileSystem fs(disk_path);
    
    // Check if the disk file exists
    if (!std::filesystem::exists(disk_path)) {
        std::cout << "Virtual disk file does not exist. Create a new one? (y/n): ";
        char response;
        std::cin >> response;
        
        if (response != 'y' && response != 'Y') {
            std::cout << "Exiting...\n";
            return 0;
        }
        
        std::cout << "Enter disk size in bytes: ";
        size_t size;
        std::cin >> size;
        
        // Consume newline
        std::cin.ignore();
        
        if (!fs.create_disk(size)) {
            std::cerr << "Failed to create virtual disk\n";
            return 1;
        }
        
        std::cout << "Virtual disk created successfully\n";
    }
    
    // Mount the disk
    if (!fs.mount_disk()) {
        std::cerr << "Failed to mount virtual disk\n";
        return 1;
    }
    
    std::cout << "Virtual disk mounted successfully\n";
    std::cout << "Type 'help' for available commands or 'exit' to quit\n";
    
    std::string input;
    bool running = true;
    
    while (running) {
        std::cout << "> ";
        std::getline(std::cin, input);
        
        if (!input.empty()) {
            running = execute_command(input, fs);
        }
    }
    
    std::cout << "Unmounting disk and exiting...\n";
    return 0;
} 