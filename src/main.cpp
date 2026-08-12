#include "bitcoinkernel_node.h"
#include <vector>
#include <string>

int main() {
    bitcoinkernel_node();

    std::vector<std::string> vec;
    vec.push_back("test_package");

    bitcoinkernel_node_print_vector(vec);
}
