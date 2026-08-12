#pragma once

#include <vector>
#include <string>


#ifdef _WIN32
  #define BITCOINKERNEL_NODE_EXPORT __declspec(dllexport)
#else
  #define BITCOINKERNEL_NODE_EXPORT
#endif

BITCOINKERNEL_NODE_EXPORT void bitcoinkernel_node();
BITCOINKERNEL_NODE_EXPORT void bitcoinkernel_node_print_vector(const std::vector<std::string> &strings);
