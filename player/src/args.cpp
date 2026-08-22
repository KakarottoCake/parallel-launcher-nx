#include "args.hpp"

#include <vector>

namespace {

// Values that arrive quoted because they contain spaces.
std::string unquote(const std::string& value) {
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
        return value.substr(1, value.size() - 2);
    return value;
}

} // namespace

PlayerArgs PlayerArgs::parse(int argc, char** argv) {
    PlayerArgs args;
    args.savePath   = "sdmc:/switch/parallel-launcher/saves";
    args.systemPath = "sdmc:/switch/parallel-launcher/system";

    std::vector<std::string> positional;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];

        auto takeValue = [&](std::string& out) {
            if (i + 1 >= argc) {
                args.error = arg + " needs a value";
                return false;
            }
            out = unquote(argv[++i]);
            return true;
        };

        if (arg == "--options") {
            if (!takeValue(args.optionsPath))
                return args;
        } else if (arg == "--return") {
            if (!takeValue(args.returnNro))
                return args;
        } else if (arg == "--return-args") {
            if (!takeValue(args.returnArgs))
                return args;
        } else if (arg == "--saves") {
            if (!takeValue(args.savePath))
                return args;
        } else if (arg == "--system") {
            if (!takeValue(args.systemPath))
                return args;
        } else if (arg.rfind("--", 0) == 0) {
            // Unknown switches are ignored rather than fatal: the launcher may
            // be newer than the player the user has on their card.
            continue;
        } else {
            positional.push_back(unquote(arg));
        }
    }

    if (positional.empty()) {
        args.error = "No ROM path given";
        return args;
    }

    args.romPath = positional.front();
    args.valid   = true;
    return args;
}

std::string PlayerArgs::buildReturnArgs() const {
    if (returnNro.empty())
        return "";

    if (!returnArgs.empty())
        return returnNro + " " + returnArgs;

    return returnNro + " --played \"" + romPath + "\"";
}
