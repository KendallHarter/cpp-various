module;

#include <cstdio>

export module module_test;

import :A;

export void test() { std::puts(greeting); }
