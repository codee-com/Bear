/* (C) 2012-2022 by László Nagy
    This file is part of Bear.

    Bear is a tool to generate compilation database for clang tooling.

    Bear is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Bear is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "Application.h"
#include "citnames/citnames-forward.h"
#include "intercept/intercept-forward.h"

#include <set>

static std::optional<std::filesystem::path> get_executable_in_path(std::string_view binary_name)
{
    std::string path = std::getenv("PATH"); // Get the PATH environment variable
    std::string_view delimiter = ":";
    size_t start = 0, end = 0;

    while ((end = path.find(delimiter, start)) != std::string::npos) {
        std::filesystem::path dir = path.substr(start, end - start); // Extract directory
        std::filesystem::path binary = dir / binary_name; // Construct full path to binary

        if (std::filesystem::exists(binary)) {
            return binary;
        }

        start = end + delimiter.size();
    }

    return std::nullopt;
}

// Return the path to the main executable, using /proc/self/exe on linux or
// otherwise falling back to the value of argv[0]
static std::optional<std::string> get_main_executable(std::string_view argv0)
{
    std::error_code ec;
#if defined(__linux__)
    std::filesystem::path selfExePath = "/proc/self/exe";
    if (std::filesystem::exists(selfExePath)) {
        // Resolve the symbolic links to get our executable
        std::filesystem::path resolvedExePath = std::filesystem::canonical(selfExePath, ec);
        if (!ec) {
            return resolvedExePath.string();
        }
    }
#endif
    // If the OS-specific detection fails or outside linux, use the path of argv0 itself

    // If argv0 does not contain a path separator, it must have been searched in PATH. Otherwise, make the
    // path canonical
    if (argv0.find(std::filesystem::path::preferred_separator) == std::string_view::npos) {
        if (std::optional<std::filesystem::path> result = get_executable_in_path(argv0)) {
            return result->string();
        }
    } else {
        std::filesystem::path resolvedArgv0Path = std::filesystem::canonical(argv0, ec);
        if (!ec) {
            return resolvedArgv0Path.string();
        }
    }
    return std::nullopt;
}

namespace {

    constexpr std::optional<std::string_view> ADVANCED_GROUP = {"advanced options"};
    constexpr std::optional<std::string_view> DEVELOPER_GROUP = {"developer options"};

    rust::Result<sys::Process::Builder>
    prepare_intercept(const flags::Arguments &arguments, const sys::env::Vars &environment, const fs::path &output) {
        auto program = arguments.as_string(cmd::bear::FLAG_BEAR);
        auto command = arguments.as_string_list(cmd::intercept::FLAG_COMMAND);
        auto library = arguments.as_string(cmd::intercept::FLAG_LIBRARY);
        auto wrapper = arguments.as_string(cmd::intercept::FLAG_WRAPPER);
        auto wrapper_dir = arguments.as_string(cmd::intercept::FLAG_WRAPPER_DIR);
        auto verbose = arguments.as_bool(flags::VERBOSE).unwrap_or(false);
        auto force_wrapper = arguments.as_bool(cmd::intercept::FLAG_FORCE_WRAPPER).unwrap_or(false);
        auto force_preload = arguments.as_bool(cmd::intercept::FLAG_FORCE_PRELOAD).unwrap_or(false);
        auto enable_network_proxy = arguments.as_bool(cmd::intercept::FLAG_ENABLE_NETWORK_PROXY).unwrap_or(false);

        return rust::merge(program, command, rust::merge(library, wrapper, wrapper_dir))
                .map<sys::Process::Builder>(
                        [&environment, &output, &verbose, &force_wrapper, &force_preload, &enable_network_proxy](auto tuple) {
                            const auto&[program, command, pack] = tuple;
                            const auto&[library, wrapper, wrapper_dir] = pack;

                            auto builder = sys::Process::Builder(program)
                                    .set_environment(environment)
                                    .add_argument(program)
                                    .add_argument("intercept")
                                    .add_argument(cmd::intercept::FLAG_LIBRARY).add_argument(library)
                                    .add_argument(cmd::intercept::FLAG_WRAPPER).add_argument(wrapper)
                                    .add_argument(cmd::intercept::FLAG_WRAPPER_DIR).add_argument(wrapper_dir)
                                    .add_argument(cmd::intercept::FLAG_OUTPUT).add_argument(output);
                            if (force_wrapper) {
                                builder.add_argument(cmd::intercept::FLAG_FORCE_WRAPPER);
                            }
                            if (force_preload) {
                                builder.add_argument(cmd::intercept::FLAG_FORCE_PRELOAD);
                            }
                            if (enable_network_proxy) {
                                builder.add_argument(cmd::intercept::FLAG_ENABLE_NETWORK_PROXY);
                            }
                            if (verbose) {
                                builder.add_argument(flags::VERBOSE);
                            }
                            builder.add_argument(cmd::intercept::FLAG_COMMAND)
                                    .add_arguments(command.begin(), command.end());
                            return builder;
                        });
    }

    rust::Result<sys::Process::Builder>
    prepare_citnames(const flags::Arguments &arguments, const sys::env::Vars &environment, const fs::path &input) {
        auto program = arguments.as_string(cmd::bear::FLAG_BEAR);
        auto output = arguments.as_string(cmd::citnames::FLAG_OUTPUT);
        auto config = arguments.as_string(cmd::citnames::FLAG_CONFIG);
        auto append = arguments.as_bool(cmd::citnames::FLAG_APPEND).unwrap_or(false);
        auto verbose = arguments.as_bool(flags::VERBOSE).unwrap_or(false);

        return rust::merge(program, output)
                .map<sys::Process::Builder>([&environment, &input, &config, &append, &verbose](auto tuple) {
                    const auto&[program, output] = tuple;

                    auto builder = sys::Process::Builder(program)
                            .set_environment(environment)
                            .add_argument(program)
                            .add_argument("citnames")
                            .add_argument(cmd::citnames::FLAG_INPUT).add_argument(input)
                            .add_argument(cmd::citnames::FLAG_OUTPUT).add_argument(output)
                            // can run the file checks, because we are on the host.
                            .add_argument(cmd::citnames::FLAG_RUN_CHECKS);
                    if (append) {
                        builder.add_argument(cmd::citnames::FLAG_APPEND);
                    }
                    if (config.is_ok()) {
                        builder.add_argument(cmd::citnames::FLAG_CONFIG).add_argument(config.unwrap());
                    }
                    if (verbose) {
                        builder.add_argument(flags::VERBOSE);
                    }
                    return builder;
                });
    }

    rust::Result<int> execute(sys::Process::Builder builder, const std::string_view &name) {
        return builder.spawn()
                .and_then<sys::ExitStatus>([](auto child) {
                    sys::SignalForwarder guard(child);
                    return child.wait();
                })
                .map<int>([](auto status) {
                    return status.code().value_or(EXIT_FAILURE);
                })
                .map_err<std::runtime_error>([&name](auto error) {
                    spdlog::warn("Running {} failed: {}", name, error.what());
                    return error;
                })
                .on_success([&name](auto status) {
                    spdlog::debug("Running {} finished. [Exited with {}]", name, status);
                });
    }
}

namespace bear {

	Command::Command(const sys::Process::Builder& intercept, const sys::Process::Builder& citnames, fs::path output) noexcept
			: ps::Command()
			, intercept_(intercept)
			, citnames_(citnames)
			, output_(std::move(output))
	{ }

	[[nodiscard]] rust::Result<int> Command::execute() const
	{
		auto result = ::execute(intercept_, "intercept");

		std::error_code error_code;
		if (fs::exists(output_, error_code)) {
			::execute(citnames_, "citnames");
			fs::remove(output_, error_code);
		}
		return result;
	}

	Application::Application()
			: ps::ApplicationFromArgs(ps::ApplicationLogConfig("bear", "br"))
	{ }

	// Since this "workaround" uses the default values of the options and those are
	// string_views, we need them to refer to an alive string value
	static std::set<std::string> StringStorage;

	static const std::string &save_path_string(const std::filesystem::path &path) {
		return *StringStorage.insert(path.string()).first;
	}

	rust::Result<flags::Arguments> Application::parse(int argc, const char **argv) const
        {
            // To make the installation portable, search for the installation
            // directory of the currently executing binary, so we can make all
            // paths relative to it.
            // By default have the installationDir empty, which is equivalent
            // to it being CWD, since the binary and library directories are in
            // relative form (./bin, ./lib). This is not very useful as bear
            // will only work at the correct installation directory, but it
            // should not happen very often because or other methods should
            // work
            std::filesystem::path installationDir;
            // Try to get the full, canonical path to argv0, which should be the
            // bear binary, and then get the installation directory by going up
            // 2 levels
            if (argc > 0 && argv[0]) {
                if (std::optional<std::string> exePath = get_main_executable(argv[0])) {
                    installationDir = *exePath;
                    installationDir = installationDir.parent_path(); // Gets /bin prefix
                    installationDir = installationDir.parent_path(); // Gets installation prefix
                }
            }
            auto installationRelativePathString = [installationDir](const std::filesystem::path &path) -> const std::string & {
                return save_path_string(installationDir / path);
            };
                const flags::Parser intercept_parser("intercept", cmd::VERSION, {
                        {cmd::intercept::FLAG_OUTPUT,               {1,  false, "path of the result file",        {cmd::intercept::DEFAULT_OUTPUT}, std::nullopt}},
                        {cmd::intercept::FLAG_FORCE_PRELOAD,        {0,  false, "force to use library preload",   std::nullopt,                     DEVELOPER_GROUP}},
                        {cmd::intercept::FLAG_FORCE_WRAPPER,        {0,  false, "force to use compiler wrappers", std::nullopt,                     DEVELOPER_GROUP}},
                        {cmd::intercept::FLAG_ENABLE_NETWORK_PROXY, {0,  false, "enable http and https proxy",    std::nullopt,                     DEVELOPER_GROUP}},
                        {cmd::intercept::FLAG_LIBRARY,              {1,  false, "path to the preload library",    {installationRelativePathString(cmd::library::DEFAULT_PATH)},     DEVELOPER_GROUP}},
                        {cmd::intercept::FLAG_WRAPPER,              {1,  false, "path to the wrapper executable", {installationRelativePathString(cmd::wrapper::DEFAULT_PATH)},     DEVELOPER_GROUP}},
                        {cmd::intercept::FLAG_WRAPPER_DIR,          {1,  false, "path to the wrapper directory",  {installationRelativePathString(cmd::wrapper::DEFAULT_DIR_PATH)}, DEVELOPER_GROUP}},
                        {cmd::intercept::FLAG_COMMAND,              {-1, true,  "command to execute",             std::nullopt,                     std::nullopt}}
                });

                const flags::Parser citnames_parser("citnames", cmd::VERSION, {
                        {cmd::citnames::FLAG_INPUT,      {1, false, "path of the input file",                    {cmd::intercept::DEFAULT_OUTPUT}, std::nullopt}},
                        {cmd::citnames::FLAG_OUTPUT,     {1, false, "path of the result file",                   {cmd::citnames::DEFAULT_OUTPUT},  std::nullopt}},
                        {cmd::citnames::FLAG_CONFIG,     {1, false, "path of the config file",                   std::nullopt,                     std::nullopt}},
                        {cmd::citnames::FLAG_APPEND,     {0, false, "append to output, instead of overwrite it", std::nullopt,                     std::nullopt}},
                        {cmd::citnames::FLAG_RUN_CHECKS, {0, false, "can run checks on the current host",        std::nullopt,                     std::nullopt}}
                });

		const flags::Parser parser("bear", cmd::VERSION, {intercept_parser, citnames_parser}, {
                        {cmd::citnames::FLAG_OUTPUT,                {1,  false, "path of the result file",                  {cmd::citnames::DEFAULT_OUTPUT},  std::nullopt}},
                        {cmd::citnames::FLAG_APPEND,                {0,  false, "append result to an existing output file", std::nullopt,                     ADVANCED_GROUP}},
                        {cmd::citnames::FLAG_CONFIG,                {1,  false, "path of the config file",                  std::nullopt,                     ADVANCED_GROUP}},
                        {cmd::intercept::FLAG_FORCE_PRELOAD,        {0,  false, "force to use library preload",             std::nullopt,                     ADVANCED_GROUP}},
                        {cmd::intercept::FLAG_FORCE_WRAPPER,        {0,  false, "force to use compiler wrappers",           std::nullopt,                     ADVANCED_GROUP}},
                        {cmd::intercept::FLAG_ENABLE_NETWORK_PROXY, {0,  false, "enable http and https proxy",              std::nullopt,                     ADVANCED_GROUP}},
                        {cmd::bear::FLAG_BEAR,                      {1,  false, "path to the bear executable",              {installationRelativePathString(cmd::bear::DEFAULT_PATH)},        DEVELOPER_GROUP}},
                        {cmd::intercept::FLAG_LIBRARY,              {1,  false, "path to the preload library",              {installationRelativePathString(cmd::library::DEFAULT_PATH)},     DEVELOPER_GROUP}},
                        {cmd::intercept::FLAG_WRAPPER,              {1,  false, "path to the wrapper executable",           {installationRelativePathString(cmd::wrapper::DEFAULT_PATH)},     DEVELOPER_GROUP}},
                        {cmd::intercept::FLAG_WRAPPER_DIR,          {1,  false, "path to the wrapper directory",            {installationRelativePathString(cmd::wrapper::DEFAULT_DIR_PATH)}, DEVELOPER_GROUP}},
                        {cmd::intercept::FLAG_COMMAND,              {-1, true,  "command to execute",                       std::nullopt,                     std::nullopt}}
		});
		return parser.parse_or_exit(argc, const_cast<const char **>(argv));
	}

        rust::Result<ps::CommandPtr> Application::command(const flags::Arguments& args, const char** envp) const
        {
                // Check if subcommand was called.
                if (args.as_string(flags::COMMAND).is_ok()) {
                        if (auto citnames = cs::Citnames(log_config_); citnames.matches(args)) {
                            return citnames.subcommand(args, envp);
                        }
                        if (auto intercept = ic::Intercept(log_config_); intercept.matches(args)) {
                            // Network proxy is disabled by default unless user explicitly enables it
                            if (!args.as_bool(cmd::intercept::FLAG_ENABLE_NETWORK_PROXY).unwrap_or(false)) {
                                for (auto proxyEnv : cmd::intercept::PROXY_ENV_VARS) {
                                    unsetenv(proxyEnv);
                                }
                            }
                            return intercept.subcommand(args, envp);
                        }
                        return rust::Err(std::runtime_error("Invalid subcommand"));
                }
                // If there were no subcommand, then just execute the two one after the other.
                // TODO: execute the two process parallel like the intercept output is the citnames input.
                //       `bear intercept -o - | bear citnames -i - -o compile_commands.json`
                auto commands = args.as_string(cmd::citnames::FLAG_OUTPUT)
                                    .map<fs::path>([](const auto& output) {
                                        return fs::path(output).replace_extension(".events.json");
                                    })
                                    .unwrap_or(fs::path(cmd::citnames::DEFAULT_OUTPUT));

                auto environment = sys::env::from(const_cast<const char**>(envp));
                auto intercept = prepare_intercept(args, environment, commands);
                auto citnames = prepare_citnames(args, environment, commands);

                return rust::merge(intercept, citnames)
                    .map<ps::CommandPtr>([&commands](const auto& tuple) {
                        const auto& [intercept, citnames] = tuple;

                        return std::make_unique<Command>(intercept, citnames, commands);
                    });
        }
}
