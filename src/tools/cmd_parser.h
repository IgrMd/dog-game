#pragma once

#include <boost/program_options.hpp>
#include <fstream>
#include <iostream>
#include <optional>
#include <vector>

namespace cmd_parser {
using namespace std::literals;

struct Args {
    std::string config_path;
    std::string root_path;
    bool randomize_spawn_points;

    size_t tick_period;
    bool has_tick_period;

    std::string state_file_path;
    bool has_state_file_path;

    size_t save_state_period;
    bool has_save_state_period;
};

[[nodiscard]] inline std::optional<Args> ParseCommandLine(int argc, const char* const argv[]) {
    namespace po = boost::program_options;

    po::options_description desc{"Allowed options"s};
    Args args;
    desc.add_options()
        ("help,h", "Show help")
        ("config-file,c", po::value(&args.config_path)->value_name("file"s), "set config file path")
        ("www-root,w", po::value(&args.root_path)->value_name("dir"s), "set static files root")
        ("tick-period,t", po::value<size_t>(&args.tick_period)->value_name("milliseconds"s), "set tick period")
        ("randomize-spawn-points,r", "spawn dogs at random positions")
        ("state-file,s", po::value(&args.state_file_path)->value_name("file"s), "set game state file path")
        ("save-state-period,p", po::value<size_t>(&args.save_state_period)->value_name("milliseconds"s), "set game state save period");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);
    if (vm.contains("help"s)) {
        std::cout << desc;
        return std::nullopt;
    }
    if (!vm.contains("config-file"s)) {
        throw std::runtime_error("Config file path has not been specified"s);
    }
    if (!vm.contains("www-root"s)) {
        throw std::runtime_error("Static files root directory path has not specified"s);
    }
    args.randomize_spawn_points = vm.contains("randomize-spawn-points"s);
    args.has_tick_period = vm.contains("tick-period"s);
    args.has_state_file_path = vm.contains("state-file");
    args.has_save_state_period = vm.contains("save-state-period");
    return args;
}

} //namespace cmd_parser