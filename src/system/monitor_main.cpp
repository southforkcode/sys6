#include "system/posix_terminal_io.h"
#include "system/system.h"

#include <fstream>
#include <iostream>

int main(int argc, char *argv[]) {
    PosixTerminalIO term;
    std::fstream tapeFile;
    std::iostream *tapeBacking = nullptr;
    if (argc > 1) {
        tapeFile.open(argv[1], std::ios::in | std::ios::out | std::ios::binary);
        if (!tapeFile.is_open()) {
            std::ofstream(argv[1], std::ios::binary).close(); // create it, then reopen for read+write
            tapeFile.open(argv[1], std::ios::in | std::ios::out | std::ios::binary);
        }
        tapeBacking = &tapeFile;
    }
    System system(term, std::cout, tapeBacking);
    system.run();
    return 0;
}
