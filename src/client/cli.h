#pragma once

#include "client.h"
#include <string>

namespace mtfss {

class CLI {
public:
    explicit CLI(Client& client);
    void run();

private:
    void process_command(const std::string& line);
    void print_help();

    Client& client_;
    bool running_;
};

} // namespace mtfss