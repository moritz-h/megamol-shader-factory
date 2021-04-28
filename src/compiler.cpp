/*
 * compiler.cpp
 *
 * Copyright (C) 2020-2021 by Universitaet Stuttgart (VISUS). Alle Rechte vorbehalten.
 */
#include "msf/compiler.h"

#include "msf/includer.h"

#include <algorithm>
#include <sstream>
#include <string>
#include <tuple>

/*
 * Adapted from : https://github.com/google/shaderc/blob/main/libshaderc_util/src/compiler.cc
 * License : Apache 2.0
 */
std::tuple<bool, int, EProfile> find_and_parse_version_string(std::string const& shader_source) {
    auto version_pos = shader_source.find("#version");
    if (version_pos == std::string::npos) {
        return std::make_tuple(false, 0, ENoProfile);
    }
    auto version_string = shader_source.substr(version_pos + std::strlen("#version"));
    version_string = version_string.substr(0, version_string.find_first_of('\n'));
    version_string.erase(std::remove(version_string.begin(), version_string.end(), ' '), version_string.end());
    int version = 0;
    std::string profile_name;
    std::istringstream(version_string) >> version >> profile_name;

    EProfile profile = ENoProfile;

    if (profile_name == glslang::ProfileName(ECoreProfile)) {
        profile = ECoreProfile;
    }
    if (profile_name == glslang::ProfileName(ECompatibilityProfile)) {
        profile = ECompatibilityProfile;
    }

    return std::make_tuple(true, version, profile);
}


std::string megamol::shaderfactory::compiler::preprocess(
    std::filesystem::path const& shader_source_path, compiler_options const& options) const {
    std::filesystem::path final_shader_source_path;
    if (std::filesystem::exists(shader_source_path)) {
        final_shader_source_path = shader_source_path;
    } else if (shader_source_path.is_relative()) {
        bool found_path = false;
        for (auto const& el : options.get_shader_paths()) {
            auto search_path = el / shader_source_path;
            if (std::filesystem::exists(search_path)) {
                if (found_path) {
                    return std::string();
                } else {
                    found_path = true;
                    final_shader_source_path = search_path;
                }
            }
        }
    } else {
        return std::string();
    }

    glslang::TShader shader(EShLangVertex);

    // auto const shader_type = get_shader_type_sc(final_shader_source_path);

    auto const shader_source = read_shader_source(final_shader_source_path);

    auto const shader_source_ptr = shader_source.data();
    auto const shader_source_length = static_cast<int>(shader_source.size());
    auto const shader_source_name = std::filesystem::canonical(final_shader_source_path).string();
    auto const shader_source_name_ptr = shader_source_name.c_str();

    shader.setStringsWithLengthsAndNames(&shader_source_ptr, &shader_source_length, &shader_source_name_ptr, 1);

    auto preamble = options.get_preamble();

    shader.setPreamble(preamble.c_str());

    shader.setEnvClient(glslang::EShClientOpenGL, glslang::EShTargetOpenGL_450);

    bool succes_version_string = false;
    int version = 0;
    EProfile profile = ENoProfile;
    std::tie(succes_version_string, version, profile) = find_and_parse_version_string(shader_source);
    if (!succes_version_string) {
        return std::string();
    }

    auto inc = options.get_includer();
    std::string output;
    auto const success =
        shader.preprocess(options.get_resource_limits(), version, profile, true, false, EShMsgDefault, &output, inc);

    if (!success) {
        throw std::runtime_error(std::string("Error preprocessing shader:\n") + shader.getInfoLog());
    }

    auto version_pos = output.find("#version");
    auto version_end = output.find_first_of('\n', version_pos) + 1;
    auto version_string = output.substr(version_pos, version_end - version_pos);
    output.erase(version_pos, version_end - version_pos);
    output.insert(0, version_string);

    return output;
}
