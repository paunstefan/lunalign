#include "commands.hpp"
#include "debayer.hpp"
#include "decode.hpp"
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

static void mux_command(std::string command, std::vector<std::string> args);

struct Command commands[] = {{"decode", 2, run_decode, "Decode a video file into FITS files."},
                             {"debayer", 2, run_debayer, "Debayer a series of images into color FITS files."}};

void process_commands(std::string script)
{
    std::stringstream command_stream(script);
    std::string command;
    char commands_del = ';';

    while (getline(command_stream, command, commands_del))
    {
        std::cout << "\"" << command << "\"\n";
        std::stringstream arg_stream(command);
        std::string arg;
        char arg_del = ' ';
        std::vector<std::string> args_vec;
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
                    args_vec.push_back(arg);
                }
            }
        }

        mux_command(command_name, args_vec);
    }
}

static void mux_command(std::string command, std::vector<std::string> args)
{
    for(int i = 0; i < sizeof(commands)/sizeof(Command); i++){
        if(commands[i].name == command){
            commands[i].runner(args);
            break;
        }
    }
}