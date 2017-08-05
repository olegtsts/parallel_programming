#include "yaml-cpp/yaml.h"
#include <vector>
#include <iostream>

int main() {
    YAML::Node config = YAML::LoadFile("yaml-test.yml");
    std::cout << config["first"]["second"].as<int>() << std::endl;
    auto vec = config["first"]["third"].as<std::vector<int>>();
    for (auto& el: vec) {
        std::cout << el << std::endl;
    }
    return 0;
}
