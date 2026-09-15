#include "cli.h"
#include "../common/protocol.h"

#include <iostream>
#include <sstream>
#include <algorithm>

namespace mtfss {

CLI::CLI(Client& client) : client_(client), running_(true) {}

void CLI::run() {
    std::cout << "\n=== MTFSS Client ===" << std::endl;
    std::cout << "Type 'help' for available commands.\n" << std::endl;

    std::string line;
    while (running_ && client_.is_connected()) {
        std::cout << "mtfss> ";
        if (!std::getline(std::cin, line)) {
            break;
        }

        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        line = line.substr(start);
        size_t end = line.find_last_not_of(" \t\r\n");
        if (end != std::string::npos) line = line.substr(0, end + 1);

        if (line.empty()) continue;

        process_command(line);
    }
}

void CLI::process_command(const std::string& line) {
    std::istringstream iss(line);
    std::string command;
    iss >> command;

    std::transform(command.begin(), command.end(), command.begin(), ::tolower);

    std::string argument;
    std::getline(iss >> std::ws, argument);

    if (command == "help") {
        print_help();
    }
    else if (command == "login") {
        if (argument.empty()) {
            std::cout << "Usage: login <username>" << std::endl;
            return;
        }
        client_.login(argument);
    }
    else if (command == "list" || command == "ls") {
        client_.list_files();
    }
    else if (command == "upload") {
        if (argument.empty()) {
            std::cout << "Usage: upload <filepath>" << std::endl;
            return;
        }
        client_.upload_file(argument);
    }
    else if (command == "download") {
        if (argument.empty()) {
            std::cout << "Usage: download <filename>" << std::endl;
            return;
        }
        client_.download_file(argument);
    }
    else if (command == "delete" || command == "rm") {
        if (argument.empty()) {
            std::cout << "Usage: delete <filename>" << std::endl;
            return;
        }
        client_.delete_file(argument);
    }
    else if (command == "rename" || command == "mv") {
        size_t space_pos = argument.find(' ');
        if (space_pos == std::string::npos) {
            std::cout << "Usage: rename <old> <new>" << std::endl;
            return;
        }
        std::string old_name = argument.substr(0, space_pos);
        std::string new_name = argument.substr(space_pos + 1);
        
        size_t start = new_name.find_first_not_of(" \t");
        if (start != std::string::npos) new_name = new_name.substr(start);

        if (old_name.empty() || new_name.empty()) {
            std::cout << "Usage: rename <old> <new>" << std::endl;
            return;
        }

        client_.rename_file(old_name, new_name);
    }
    else if (command == "quit" || command == "exit") {
        client_.quit();
        running_ = false;
    }
    else {
        std::cout << "Unknown command: " << command << std::endl;
        std::cout << "Type 'help' for available commands." << std::endl;
    }
}

void CLI::print_help() {
    std::cout << "\nAvailable commands:" << std::endl;
    std::cout << "  login <username>    - Set your username (required first)" << std::endl;
    std::cout << "  list / ls           - List your files" << std::endl;
    std::cout << "  upload <filepath>   - Upload a file" << std::endl;
    std::cout << "  download <filename> - Download a file" << std::endl;
    std::cout << "  delete <filename>   - Delete a file" << std::endl;
    std::cout << "  rename <old> <new>  - Rename a file" << std::endl;
    std::cout << "  quit / exit         - Disconnect and exit" << std::endl;
    std::cout << std::endl;
}

} // namespace mtfss