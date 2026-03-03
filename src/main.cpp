/*
 * lunalign - A tool for lunar astrophotography processing
 * Copyright (C) 2025-2026 Stefan Paun
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <filesystem>
#include <fstream>
#include <print>
#include <string>
#include <utility>
#include <opencv2/core/utils/logger.hpp>

#include "commands.hpp"
#include "result.hpp"

const char *helpstring = "Usage: lunalign <commands>/<script_file>";

int main(int argc, const char *argv[])
{
    if (argc != 2)
    {
        std::println("{}", helpstring);
        return 1;
    }

    std::string arg = argv[1];

    if (arg == "-h" || arg == "--help")
    {
        std::println("{}", helpstring);
        return 0;
    }

    cv::utils::logging::setLogLevel(cv::utils::logging::LOG_LEVEL_ERROR);

    la_result result = la_result::Error;
    if (std::filesystem::is_regular_file(arg))
    {
        std::ifstream file(arg);
        const auto file_size = std::filesystem::file_size(arg);
        if (!file.is_open())
        {
            std::println("Error: Could not open file: {}", arg);
            return -1;
        }
        std::string content(file_size, '\0');
        file.read(content.data(), file_size);
        result = process_commands(content);
    }
    else
    {
        result = process_commands(arg);
    }

    return std::to_underlying(result);
}