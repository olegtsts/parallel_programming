#include <exception>
#include <stdexcept>
#include <string>
#include <iostream>
#include <cxxabi.h>
#include <unwind.h>
#include <backtrace.h>
#include <dlfcn.h>
#include <cstdlib>
#include <sstream>

struct StackCrowlState {
    unsigned int skip;
    unsigned int total_count = 0;
    unsigned int output_count = 0;
    std::stringstream ss = std::stringstream();
};

struct pc_data {
    std::string function;
    std::string filename;
    std::size_t line;
};

std::string GetString(const char * ptr) {
    if (ptr != nullptr) {
        return std::string(ptr);
    } else {
        return std::string();
    }
}

int fill_callback(void *data, uintptr_t /*pc*/, const char *filename, int lineno, const char *function) {
    pc_data& d = *static_cast<pc_data*>(data);
    d.filename = GetString(filename);
    d.function = GetString(function);
    d.line = lineno;
    return 0;
}

void error_callback(void* /*data*/, const char* /*msg*/, int /*errnum*/) {}

static _Unwind_Reason_Code trace_func(_Unwind_Context* context, void* void_state)
{
    StackCrowlState *state = (StackCrowlState *)void_state;
    auto ip = _Unwind_GetIP(context);
    if (state->total_count++ >= state->skip && ip) {
        ::backtrace_state* bt_state = ::backtrace_create_state(0, 0, error_callback, 0);
        pc_data data;
        ::backtrace_pcinfo(bt_state, reinterpret_cast<uintptr_t>(ip - 5), fill_callback, error_callback, &data);
        if (data.filename.size() > 0) {
            int status = 0;
            std::size_t size = 0;

            std::string demangled_function = GetString(abi::__cxa_demangle( data.function.c_str(), NULL, &size, &status ));
            if (demangled_function.size() == 0) {
                demangled_function = data.function;
            }
            state->ss << state->output_count++ << ": " << demangled_function << " at " << data.filename << ":" << data.line << std::endl;
        } else {
            ::Dl_info dl_info;
            ::dladdr((void *)ip, &dl_info);
            state->ss << state->output_count++ << ": " << (void *)ip << " in " << dl_info.dli_fname << std::endl;
        }
    }
    return(_URC_NO_REASON);
}


std::string BackTraceUsingUnwind(const unsigned int skip_count) {
  StackCrowlState state{skip_count + 1};
  _Unwind_Backtrace(trace_func,&state);
  return state.ss.str();
}

class MyException {
public:
    MyException() {
        try {
            exception_backtrace = BackTraceUsingUnwind(1);
        } catch (...) {
            try {
                exception_backtrace = "Unable to get backtrace";
            } catch (...) {
            }
        }
    }

    const std::string& GetStackTrace() const {
        return exception_backtrace;
    }
private:
    std::string exception_backtrace;
};

void Func2() {
    throw MyException();
}

void Func() {
    Func2();
    //
    //
    std::cout << "Another action\n";
}

int main() {
    try {
        Func();
        //
        //
    } catch(const MyException& e) {
        std::cout << "Exception caught\n";
        std::cout << e.GetStackTrace();
    }
    std::cout << "Finished\n";
    return 0;
}
