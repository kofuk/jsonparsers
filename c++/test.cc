#include <iostream>

#include "parse.h"

int main(int argc, char **argv) {
    std::ifstream ifs(argv[1]);
    json::JSON_File result;
    if (argc < 2) {
        result = json::parse(std::cin);
    } else {
        result = json::parse(ifs);
    }
    if (!result.ok()) {
        std::cout << "Parse error.\n";
        return 1;
    }
    std::cout << result.get_root()->to_string() << '\n';
}
