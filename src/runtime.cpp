#include <cstdlib.hpp>
#include <runtime.hpp>

extern "C" void userland_entry() noexcept {
    std::exit(static_cast<std::uint64_t>(main()));
}
