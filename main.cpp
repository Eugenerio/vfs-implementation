#include "filesystem.h"
#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <iomanip>

#define COLOR_RESET "\033[0m"
#define COLOR_RED "\033[31m"
#define COLOR_GREEN "\033[32m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_BLUE "\033[34m"
#define COLOR_CYAN "\033[36m"
#define COLOR_BOLD "\033[1m"

void print_usage()
{
    std::cout << COLOR_BOLD << COLOR_CYAN << "Available commands:" << COLOR_RESET << "\n";
    std::cout << COLOR_YELLOW << "  mkdir <path>" << COLOR_RESET << "      - Create a directory\n";
    std::cout << COLOR_YELLOW << "  rmdir <path>" << COLOR_RESET << "      - Remove a directory\n";
    std::cout << COLOR_YELLOW << "  copyto <virt_path> <sys_path>" << COLOR_RESET << " - Copy a file from virtual disk to system\n";
    std::cout << COLOR_YELLOW << "  copyfrom <sys_path> <virt_path>" << COLOR_RESET << " - Copy a file from system to virtual disk\n";
    std::cout << COLOR_YELLOW << "  ls <path>" << COLOR_RESET << "         - List directory contents\n";
    std::cout << COLOR_YELLOW << "  link <target> <link_path>" << COLOR_RESET << " - Create a hard link\n";
    std::cout << COLOR_YELLOW << "  rm <path>" << COLOR_RESET << "          - Remove a file or link\n";
    std::cout << COLOR_YELLOW << "  append <path> <bytes>" << COLOR_RESET << " - Add bytes to a file\n";
    std::cout << COLOR_YELLOW << "  truncate <path> <bytes>" << COLOR_RESET << " - Truncate a file by bytes\n";
    std::cout << COLOR_YELLOW << "  usage" << COLOR_RESET << "              - Show disk usage\n";
    std::cout << COLOR_YELLOW << "  clear" << COLOR_RESET << "              - Clear the screen\n";
    std::cout << COLOR_YELLOW << "  help" << COLOR_RESET << "               - Show this help\n";
    std::cout << COLOR_YELLOW << "  exit" << COLOR_RESET << "               - Exit the program\n";
}

void print_error(const std::string &msg)
{
    std::cout << COLOR_RED << "Error: " << msg << COLOR_RESET << "\n";
}

void print_success(const std::string &msg)
{
    std::cout << COLOR_GREEN << msg << COLOR_RESET << "\n";
}

void print_info(const std::string &msg)
{
    std::cout << COLOR_CYAN << msg << COLOR_RESET << "\n";
}

bool execute_command(const std::string &input, FileSystem &fs)
{
    std::istringstream iss(input);
    std::string cmd;

    iss >> cmd;

    if (cmd == "exit")
    {
        return false;
    }
    else if (cmd == "help")
    {
        print_usage();
    }
    else if (cmd == "clear")
    {
        // Clear screen using ANSI escape sequence (works in most terminals)
        std::cout << "\033[2J\033[1;1H";
    }
    else if (cmd == "mkdir")
    {
        std::string path;
        iss >> path;

        if (path.empty())
        {
            print_error("Missing path parameter");
            return true;
        }

        print_info("Trying to create directory '" + path + "'");

        if (fs.create_directory(path))
        {
            print_success("Directory created successfully");
        }
        else
        {
            print_error("Failed to create directory");
        }
    }
    else if (cmd == "rmdir")
    {
        std::string path;
        iss >> path;

        if (path.empty())
        {
            print_error("Missing path parameter");
            return true;
        }

        std::cout << COLOR_YELLOW << "Are you sure you want to remove directory '" << path << "'? (y/n): " << COLOR_RESET;
        char confirm;
        std::cin >> confirm;
        std::cin.ignore();

        if (confirm != 'y' && confirm != 'Y')
        {
            print_info("Cancelled");
            return true;
        }

        if (fs.remove_directory(path))
        {
            print_success("Directory removed successfully");
        }
        else
        {
            print_error("Failed to remove directory");
        }
    }
    else if (cmd == "copyto")
    {
        std::string virt_path, sys_path;
        iss >> virt_path >> sys_path;

        if (virt_path.empty() || sys_path.empty())
        {
            print_error("Missing parameters");
            return true;
        }

        print_info("Copying from virtual disk to system...");

        if (fs.copy_to_system(virt_path, sys_path))
        {
            print_success("File copied successfully");
        }
        else
        {
            print_error("Failed to copy file");
        }
    }
    else if (cmd == "copyfrom")
    {
        std::string sys_path, virt_path;
        iss >> sys_path >> virt_path;

        if (sys_path.empty() || virt_path.empty())
        {
            print_error("Missing parameters");
            return true;
        }

        print_info("Trying to copy from '" + sys_path + "' to '" + virt_path + "'");

        std::ifstream file(sys_path);
        if (!file.good())
        {
            print_error("System file does not exist");
            return true;
        }

        if (fs.copy_from_system(sys_path, virt_path))
        {
            print_success("File copied successfully");
        }
        else
        {
            print_error("Failed to copy file");
        }
    }
    else if (cmd == "ls")
    {
        std::string path;
        iss >> path;

        if (path.empty())
        {
            path = "/";
        }

        auto entries = fs.list_directory(path);

        if (entries.empty())
        {
            print_info("Directory is empty or does not exist");
        }
        else
        {
            uint32_t total_size = 0;
            std::cout << COLOR_BOLD << "Contents of " << path << ":" << COLOR_RESET << "\n";
            std::cout << COLOR_CYAN << std::left << std::setw(30) << "Name" << std::right << std::setw(10) << "Size (B)" << COLOR_RESET << "\n";
            std::cout << std::string(40, '-') << "\n";

            for (const auto &entry : entries)
            {
                std::cout << std::left << std::setw(30) << entry.first
                          << std::right << std::setw(10) << entry.second << "\n";
                total_size += entry.second;
            }

            std::cout << std::string(40, '-') << "\n";
            std::cout << COLOR_CYAN << "Total size: " << total_size << " bytes" << COLOR_RESET << "\n";
        }
    }
    else if (cmd == "link")
    {
        std::string target, link_path;
        iss >> target >> link_path;

        if (target.empty() || link_path.empty())
        {
            print_error("Missing parameters");
            return true;
        }

        if (fs.create_link(target, link_path))
        {
            print_success("Link created successfully");
        }
        else
        {
            print_error("Failed to create link");
        }
    }
    else if (cmd == "rm")
    {
        std::string path;
        iss >> path;

        if (path.empty())
        {
            print_error("Missing path parameter");
            return true;
        }

        std::cout << COLOR_YELLOW << "Are you sure you want to remove file/link '" << path << "'? (y/n): " << COLOR_RESET;
        char confirm;
        std::cin >> confirm;
        std::cin.ignore();

        if (confirm != 'y' && confirm != 'Y')
        {
            print_info("Cancelled");
            return true;
        }

        if (fs.remove_file(path))
        {
            print_success("File removed successfully");
        }
        else
        {
            print_error("Failed to remove file");
        }
    }
    else if (cmd == "append")
    {
        std::string path;
        size_t bytes;
        iss >> path >> bytes;

        if (path.empty() || bytes == 0)
        {
            print_error("Missing or invalid parameters");
            return true;
        }

        if (fs.append_to_file(path, bytes))
        {
            print_success(std::to_string(bytes) + " bytes appended successfully");
        }
        else
        {
            print_error("Failed to append to file");
        }
    }
    else if (cmd == "truncate")
    {
        std::string path;
        size_t bytes;
        iss >> path >> bytes;

        if (path.empty() || bytes == 0)
        {
            print_error("Missing or invalid parameters");
            return true;
        }

        if (fs.truncate_file(path, bytes))
        {
            print_success("File truncated by " + std::to_string(bytes) + " bytes successfully");
        }
        else
        {
            print_error("Failed to truncate file");
        }
    }
    else if (cmd == "usage")
    {
        auto usage = fs.get_disk_usage();

        std::cout << COLOR_BOLD << "Disk usage:" << COLOR_RESET << "\n";
        std::cout << COLOR_CYAN << "Used: " << usage.first << " blocks ("
                  << usage.first * BLOCK_SIZE << " bytes)\n";
        std::cout << "Total: " << usage.second << " blocks ("
                  << usage.second * BLOCK_SIZE << " bytes)\n";
        std::cout << "Free: " << (usage.second - usage.first) << " blocks ("
                  << (usage.second - usage.first) * BLOCK_SIZE << " bytes)\n";
        std::cout << "Usage: " << std::fixed << std::setprecision(2)
                  << (static_cast<double>(usage.first) / usage.second * 100) << "%" << COLOR_RESET << "\n";
    }
    else
    {
        std::cout << COLOR_RED << "Unknown command: " << cmd << COLOR_RESET << "\n";
        print_usage();
    }

    return true;
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        std::cerr << "Usage: " << argv[0] << " <disk_file>\n";
        return 1;
    }

    std::string disk_path = argv[1];
    FileSystem fs(disk_path);

    // Check if the disk file exists
    std::ifstream file(disk_path);
    if (!file.good())
    {
        std::cout << "Virtual disk file does not exist. Create a new one? (y/n): ";
        char response;
        std::cin >> response;

        if (response != 'y' && response != 'Y')
        {
            std::cout << "Exiting...\n";
            return 0;
        }

        std::cout << "Enter disk size in bytes: ";
        size_t size;
        std::cin >> size;

        // Consume newline
        std::cin.ignore();

        if (!fs.create_disk(size))
        {
            std::cerr << "Failed to create virtual disk\n";
            return 1;
        }

        std::cout << "Virtual disk created successfully\n";
    }

    // Mount the disk
    if (!fs.mount_disk())
    {
        std::cerr << "Failed to mount virtual disk\n";
        return 1;
    }

    std::cout << COLOR_GREEN << "Virtual disk mounted successfully" << COLOR_RESET << "\n";
    std::cout << COLOR_CYAN << "Type 'help' for available commands or 'exit' to quit" << COLOR_RESET << "\n";

    std::string input;
    bool running = true;

    while (running)
    {
        std::cout << COLOR_BOLD << "> " << COLOR_RESET;
        std::getline(std::cin, input);

        if (!input.empty())
        {
            running = execute_command(input, fs);
        }
    }

    std::cout << COLOR_YELLOW << "Unmounting disk and exiting..." << COLOR_RESET << "\n";
    return 0;
}