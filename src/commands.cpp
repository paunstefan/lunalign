#include "commands.hpp"
#include "debayer.hpp"
#include "decode.hpp"
#include "rate.hpp"
#include "registration.hpp"
#include "result.hpp"
#include "stack.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <print>
#include <sstream>
#include <string>
#include <unordered_map>

static la_result mux_command(const std::string &command, std::unordered_map<std::string, std::string> args);
static void insert_in_map(std::unordered_map<std::string, std::string> &map, const std::string &arg);

const std::vector<Command> commands = {
    {"decode",
     {{"in", true, ""}, {"out", false, "process/decoded"}},
     run_decode,
     "Decode a video file into FITS files."},
    {"debayer",
     {{"in", true, ""}, {"out", false, "process/debayered"}},
     run_debayer,
     "Debayer a series of images into color FITS files."},
    {"rate",
     {{"in", true, ""}, {"percent", true, ""}, {"out", false, "process/rating_out"}},
     run_rate,
     "Rate the clarity of the images and copy the best ones."},
    {"register",
     {{"in", true, ""},
      {"reference", true, ""},
      {"out", false, "process/registered"},
      {"rotation", false, "0"},
      {"highpass", false, "1"},
      {"scaling", false, "0"}},
     run_registration,
     "Regsiter the frames to a given reference frame."},
    {"stack",
     {{"in", true, ""},
      {"out", false, "process/stacked.fits"},
      {"method", false, "sigma"},
      {"sigma", false, "2.5"},
      {"weighted", false, "0"}},
     run_stack,
     "Stack registered frames into a single image."},
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

static la_result validate_and_run(const Command &cmd, std::unordered_map<std::string, std::string> &args)
{
    for (const auto &spec : cmd.args)
    {
        if (spec.required && !args.contains(spec.name))
        {
            std::println(std::cerr, "Error: Required argument '{}' for '{}' not present!", spec.name, cmd.name);
            return la_result::Error;
        }
        if (!spec.required && !args.contains(spec.name) && !spec.default_value.empty())
        {
            args[spec.name] = spec.default_value;
        }
    }

    auto start = std::chrono::high_resolution_clock::now();

    auto result = cmd.runner(args);

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end - start;
    std::println(std::cerr, "Elapsed time: {}ms", elapsed.count());

    return result;
}

static la_result mux_command(const std::string &command, std::unordered_map<std::string, std::string> args)
{

    auto it = std::ranges::find_if(commands, [&](const Command &c) { return c.name == command; });
    if (it == commands.end())
    {
        std::println(std::cerr, "Error: command '{}' not valid.", command);
        return la_result::Error;
    }
    return validate_and_run(*it, args);
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