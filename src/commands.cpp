#include "commands.hpp"
#include "debayer.hpp"
#include "decode.hpp"
#include "rate.hpp"
#include "result.hpp"
#include <iostream>
#include <print>
#include <sstream>
#include <string>
#include <unordered_map>

static la_result mux_command(const std::string &command, std::unordered_map<std::string, std::string> args);
static void insert_in_map(std::unordered_map<std::string, std::string> &map, const std::string &arg);

struct Command commands[] = {
    {"decode", 2, run_decode, "Decode a video file into FITS files."},
    {"debayer", 2, run_debayer, "Debayer a series of images into color FITS files."},
    {"rate", 3, run_rate, "Rate the clarity of the images and copy the best ones."},
};

la_result process_commands(std::string script)
{
    std::stringstream command_stream(script);
    std::string command;
    char commands_del = ';';

    while (getline(command_stream, command, commands_del))
    {
        std::stringstream arg_stream(command);
        std::string arg;
        char arg_del = ' ';
        std::unordered_map<std::string, std::string> args_map;
        bool first = true;
        std::string command_name;

        while (getline(arg_stream, arg, arg_del))
        {
            if (!arg.empty())
            {
                if (first)
                {
                    command_name = arg;
                    first = false;
                }
                else
                {
                    insert_in_map(args_map, arg);
                }
            }
        }
        std::println("{}: {}", command_name, args_map);
        la_result result = mux_command(command_name, args_map);
        if (result == la_result::Error)
        {
            return result;
        }
    }
    return la_result::Ok;
}

static la_result mux_command(const std::string &command, std::unordered_map<std::string, std::string> args)
{
    bool found = false;
    for (int i = 0; i < sizeof(commands) / sizeof(Command); i++)
    {
        if (commands[i].name == command)
        {
            found = true;
            la_result result = commands[i].runner(args);
            if (result == la_result::Error)
            {
                return result;
            }
            break;
        }
    }

    if (!found)
    {
        std::println(std::cerr, "Error: command '{}' not valid.", command);
        return la_result::Error;
    }

    return la_result::Ok;
}

static void insert_in_map(std::unordered_map<std::string, std::string> &map, const std::string &arg)
{
    size_t equal_pos = arg.find('=');

    if (arg.rfind('-', 0) == 0 && equal_pos != std::string::npos)
    {
        std::string key = arg.substr(1, equal_pos - 1);
        std::string value = arg.substr(equal_pos + 1);
        map[key] = value;
    }
}