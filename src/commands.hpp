#include <string>
#include <functional>
#include <vector>

struct Command {
    std::string name;
    int number_args;
    std::function<void(std::vector<std::string>)> runner;
    std::string help;
};

void process_commands(std::string script);