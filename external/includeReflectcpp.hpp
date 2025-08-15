#pragma once

#ifdef WIN32
    #pragma warning(push)
    #pragma warning(disable : 5045)
    #pragma warning(disable : 4514)
    #pragma warning(disable : 4820)
    #pragma warning(disable : 4324)
#else
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wunused-parameter"
    #pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif

#include <rfl.hpp>
#include <rfl/json.hpp>

#ifdef WIN32
    #pragma warning(pop)
#else
    #pragma GCC diagnostic pop
#endif