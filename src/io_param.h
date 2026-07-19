#pragma once

#include <string>
#include <fstream>
#include <cstddef>
#include <cstdio>

class IOParam {
public:
    static constexpr const char* BASE_NAME = "best_io_param.bin";

    static std::string tag_filename(const std::string& base, const std::string& tag) {
        if (tag.empty()) return base;
        auto dot = base.rfind('.');
        if (dot != std::string::npos) {
            return base.substr(0, dot) + "_" + tag + base.substr(dot);
        } else {
            return base + "_" + tag;
        }
    }

    static std::string filename(const std::string& tag = "") {
        return tag_filename(BASE_NAME, tag);
    }

    static bool read(double* out, std::size_t count, const std::string& tag = "") {
        auto fn = filename(tag);
        std::ifstream ifs(fn, std::ios::binary);
        if (!ifs.good()) return false;
        ifs.read(reinterpret_cast<char*>(out), static_cast<std::streamsize>(count * sizeof(double)));
        bool ok = ifs.gcount() == static_cast<std::streamsize>(count * sizeof(double));
        ifs.close();
        return ok;
    }

    static bool write(const double* data, std::size_t count, const std::string& tag = "") {
        auto fn = filename(tag);
        std::ofstream ofs(fn, std::ios::binary);
        if (!ofs.good()) return false;
        ofs.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(count * sizeof(double)));
        bool ok = ofs.good();
        ofs.close();
        return ok;
    }

private:
    IOParam() = delete; // static-only class
};
