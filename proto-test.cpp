#include "proto.pb.h"
#include <iostream>
#include <string>
#include <fstream>

int main() {
    tutorial::Person person;
    person.set_name("abc");
    person.set_x(123);
    person.mutable_example()->operator[](4) = 11;
    person.mutable_example()->operator[](5) = 10;
    std::string str_person;
    person.SerializeToString(&str_person);
    std::cout << "Size = " << str_person.size() << std::endl;
    tutorial::Person parsed_person;
    parsed_person.ParseFromString(str_person);
    std::cout << parsed_person.name() << std::endl;
    std::cout << parsed_person.x() << std::endl;
    std::cout << parsed_person.DebugString() << std::endl;
    std::cout << parsed_person.example().at(4) << std::endl;

    std::cout << "Output from file\n";
    std::ifstream fh("proto_dump.txt");
    tutorial::Person read_person;
    read_person.ParseFromIstream(&fh);
    std::cout << read_person.name() << std::endl;
    std::cout << read_person.x() << std::endl;
    std::cout << read_person.DebugString() << std::endl;
    auto it = read_person.example().find(4);
    if (it != read_person.example().end()) {
        std::cout << it->second << std::endl;
    } else {
        std::cout << "Cannot find element\n" << std::endl;
    }
}
//protoc proto.proto --js_out=library=person,binary:. --cpp_out=.
//g++-5 proto-test.cpp proto.pb.cc -lprotobuf
