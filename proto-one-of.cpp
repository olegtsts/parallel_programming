#include "proto-one-of.pb.h"
#include <string>
#include <iostream>

std::string GetFirstMessage() {
    SampleMessage message;
    message.set_message_type(SampleMessage::First);
    message.mutable_first_message()->set_first(123);

    std::string new_string;
    message.SerializeToString(&new_string);

    return new_string;
}

std::string GetSecondMessage() {
    SampleMessage message;
    message.set_message_type(SampleMessage::Second);
    auto second_message = message.mutable_second_message();
    second_message->set_second(456);
    second_message->set_third(789);

    std::string new_string;
    message.SerializeToString(&new_string);

    return new_string;
}

void ReadCommonMessage(const std::string& new_string) {
    SampleMessage read_message;
    read_message.ParseFromString(new_string);
    if (read_message.message_type() == SampleMessage::First) {
        std::cout << "Data from first message: " << read_message.first_message().first() << std::endl;
    } else {
        std::cout << "Data from second message: " << read_message.second_message().second()
            << " " << read_message.second_message().third() << std::endl;
    }
}

int main() {
    ReadCommonMessage(GetFirstMessage());
    ReadCommonMessage(GetSecondMessage());
    return 0;
}
//protoc proto-one-of.proto --cpp_out=.
//g++-7 proto-one-of.cpp proto-one-of.pb.cc -lprotobuf
