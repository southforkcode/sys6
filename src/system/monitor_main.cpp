#include "system/posix_terminal_io.h"
#include "system/system.h"

#include <iostream>

int main() {
    PosixTerminalIO term;
    System system(term, std::cout);
    system.run();
    return 0;
}
