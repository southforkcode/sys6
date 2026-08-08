#include "monitor_firmware.h"

#include <cctype>
#include <stdexcept>

namespace monitor {

namespace {
int hexDigitValue(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    throw std::invalid_argument("loadRoutine: invalid hex character");
}
} // namespace

void loadRoutine(ROM &rom, uint16_t busAddr, const std::string &hex) {
    std::string digits;
    for (char c : hex) {
        if (!std::isspace(static_cast<unsigned char>(c))) {
            digits.push_back(c);
        }
    }
    if (digits.size() % 2 != 0) {
        throw std::invalid_argument("loadRoutine: hex string has odd digit count");
    }

    uint16_t offset = static_cast<uint16_t>(busAddr - kRomBase);
    for (size_t i = 0; i < digits.size(); i += 2) {
        auto byte = static_cast<uint8_t>((hexDigitValue(digits[i]) << 4) | hexDigitValue(digits[i + 1]));
        rom.load(offset, byte);
        offset = static_cast<uint16_t>(offset + 1);
    }
}

// GETCHAR ($C000): busy-waits for RXRDY, reads and returns DATA in A. Does
// not echo -- callers that want an echo call PUTCHAR themselves.
const std::string kGetCharHex =
    "AD 00 80" // GETCHAR: LDA $8000        (TTY STATUS)
    "29 01"    //   AND #$01                (RXRDY bit)
    "F0 F9"    //   BEQ GETCHAR             (loop while not ready)
    "AD 01 80" //   LDA $8001               (TTY DATA -- clears RXRDY)
    "60";      //   RTS

// PUTCHAR ($C100): writes A to TTY DATA. TXRDY is always 1 by design, so
// there's nothing to poll before writing.
const std::string kPutCharHex =
    "8D 01 80" // PUTCHAR: STA $8001
    "60";      //   RTS

// PRINT_HEX_BYTE ($C200): prints A as two hex ASCII chars, high nibble
// first. PRINT_NIBBLE ($C220) is its private helper (A in [0..15]); both
// nibble paths tail-call PUTCHAR via JMP so PUTCHAR's own RTS returns
// straight to PRINT_HEX_BYTE's caller.
const std::string kPrintHexByteHex =
    "48"       // PRINT_HEX_BYTE: PHA       (save original byte)
    "4A"       //   LSR A
    "4A"       //   LSR A
    "4A"       //   LSR A
    "4A"       //   LSR A                   (A = high nibble)
    "20 20 C2" //   JSR PRINT_NIBBLE
    "68"       //   PLA                     (restore original byte)
    "29 0F"    //   AND #$0F                (A = low nibble)
    "20 20 C2" //   JSR PRINT_NIBBLE
    "60";      //   RTS

const std::string kPrintNibbleHex =
    "C9 0A"    // PRINT_NIBBLE: CMP #$0A
    "90 05"    //   BCC DIGIT               (A<10 -> carry clear -> digit path)
    "69 36"    //   ADC #$36                (carry=1 here: A+0x36+1='A'..'F')
    "4C 00 C1" //   JMP PUTCHAR             (tail call)
    "69 30"    // DIGIT: ADC #$30           (carry=0 here: A+0x30='0'..'9')
    "4C 00 C1"; //   JMP PUTCHAR            (tail call)

// PRINT_STRING ($C300): prints the null-terminated string pointed to by
// STRPTR ($FC/$FD).
const std::string kPrintStringHex =
    "A0 00"    // PRINT_STRING: LDY #$00
    "B1 FC"    // LOOP: LDA ($FC),Y
    "F0 07"    //   BEQ DONE
    "20 00 C1" //   JSR PUTCHAR
    "C8"       //   INY
    "4C 02 C3" //   JMP LOOP
    "60";      // DONE: RTS

// HEXVAL ($C700): converts an ASCII hex char in A to its nibble value in A.
// Carry clear = valid, carry set = invalid (not 0-9/A-F/a-f).
const std::string kHexValHex =
    "C9 30" // HEXVAL: CMP #$30
    "90 23" //   BCC INVALID
    "C9 3A" //   CMP #$3A
    "B0 05" //   BCS CHECKALPHA
    "38"    //   SEC
    "E9 30" //   SBC #$30
    "18"    //   CLC
    "60"    //   RTS
    "C9 41" // CHECKALPHA: CMP #$41
    "90 16" //   BCC INVALID
    "C9 47" //   CMP #$47
    "B0 05" //   BCS CHECKLOWER
    "38"    //   SEC
    "E9 37" //   SBC #$37
    "18"    //   CLC
    "60"    //   RTS
    "C9 61" // CHECKLOWER: CMP #$61
    "90 09" //   BCC INVALID
    "C9 67" //   CMP #$67
    "B0 05" //   BCS INVALID
    "38"    //   SEC
    "E9 57" //   SBC #$57
    "18"    //   CLC
    "60"    //   RTS
    "38"    // INVALID: SEC
    "60";   //   RTS

} // namespace monitor
