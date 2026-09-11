#pragma once

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>

namespace sigproc::cli {

inline constexpr int kDefaultGulp = 16384;

[[nodiscard]] inline bool is_stdio_path(std::string_view path) {
    return path.empty() || path == "-";
}

inline void configure_app(CLI::App& app) {
    // Original SIGPROC flags are single-dash long names (`-tsamp`,
    // `-telescope`). Default `-h` would collide with `-headersize` /
    // `-headerless`.
    app.allow_non_standard_option_names();
    app.set_help_flag("--help", "Print this help message and exit");
}

inline void init_logging(bool verbose, bool debug) {
    if (debug) {
        spdlog::set_level(spdlog::level::debug);
    } else if (verbose) {
        spdlog::set_level(spdlog::level::info);
    } else {
        spdlog::set_level(spdlog::level::warn);
    }
}

inline void add_log_flags(CLI::App& app, bool& verbose, bool& debug) {
    app.add_flag("-v,--verbose", verbose, "Verbose (info) logging on stderr");
    app.add_flag("--debug", debug, "Debug logging on stderr");
}

inline void add_input_file(CLI::App& app, std::string& filename) {
    app.add_option("filename", filename,
                   "Input filterbank file (default: stdin)")
        ->check([](const std::string& path) -> std::string {
            if (is_stdio_path(path)) {
                return {};
            }
            std::ifstream in(path);
            if (!in) {
                return "File does not exist: " + path;
            }
            return {};
        });
}

inline void add_output_file(CLI::App& app, std::string& outfile) {
    app.add_option("-o,--outfile", outfile,
                   "Output file (default: stdout; '-' is stdout)");
}

inline void add_gulp_flag(CLI::App& app, int& gulp) {
    app.add_option("-g,--gulp", gulp,
                   "Time samples to read per gulp (def=16384)");
}

/// Binary stdout when `path` is empty or "-"; otherwise an owned ofstream.
class OutputStream {
public:
    explicit OutputStream(const std::string& path) {
        if (!is_stdio_path(path)) {
            m_owned.open(path, std::ios::binary);
            if (!m_owned) {
                throw std::runtime_error("Cannot open output file: " + path);
            }
            m_out = &m_owned;
        }
    }

    [[nodiscard]] std::ostream& get() const noexcept { return *m_out; }

private:
    std::ofstream m_owned;
    std::ostream* m_out = &std::cout;
};

} // namespace sigproc::cli
