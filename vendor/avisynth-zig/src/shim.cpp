// C shim over avisynth_c_api_loader's C++ interface (std::span<std::string_view>)
// so the Zig module can call it through plain extern "C" functions.

#include <string_view>
#include <vector>

#include "avs_c_api_loader.hpp"

extern "C" {

const avisynth_c_api_pointers* avsz_get_api(AVS_ScriptEnvironment* env, int required_interface_version,
    int required_bugfix_version, const char* const* required_names, size_t required_names_count)
{
    std::vector<std::string_view> names;
    names.reserve(required_names_count);
    for (size_t i{0}; i < required_names_count; ++i)
        names.emplace_back(required_names[i]);

    return avisynth_c_api_loader::get_api(
        env, required_interface_version, required_bugfix_version, std::span<const std::string_view>{names});
}

const char* avsz_get_last_error(void)
{
    return avisynth_c_api_loader::get_last_error();
}

} // extern "C"
