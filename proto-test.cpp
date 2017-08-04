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
    tutorial::Person parsed_person;
    parsed_person.ParseFromString(str_person);
    std::cout << parsed_person.name() << std::endl;
    std::cout << parsed_person.x() << std::endl;
    std::cout << parsed_person.DebugString() << std::endl;
    std::cout << parsed_person.example().at(4) << std::endl;

    std::ifstream fh("proto_dump.txt");
    tutorial::Person read_person;
    read_person.ParseFromIstream(&fh);
    std::cout << read_person.name() << std::endl;
    std::cout << read_person.x() << std::endl;
    std::cout << read_person.DebugString() << std::endl;
    std::cout << read_person.example().at(4) << std::endl;

}
//protoc proto.proto --js_out=library=person,binary:. --cpp_out=.
//g++-5 proto-test.cpp proto.pb.cc -lprotobuf
