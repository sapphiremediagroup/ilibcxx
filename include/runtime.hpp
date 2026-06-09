#pragma once

extern "C" [[noreturn]] void userland_entry(unsigned long long argc, char** argv, char** envp) noexcept;

int main(int argc, char** argv);
