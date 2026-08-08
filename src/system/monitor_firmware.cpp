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

// PARSE_ADDR ($C500): parses hex digits from LINEBUF starting at LINEPOS
// into ADDR ($F0/$F1, 16-bit, zero-extended), advancing LINEPOS past each
// consumed digit, stopping at the first non-hex character. Carry set on
// return = zero digits consumed (invalid).
const std::string kParseAddrHex =
    "A9 00"    // PARSE_ADDR: LDA #$00
    "85 F0"    //   STA $F0                 (ADDRLO = 0)
    "85 F1"    //   STA $F1                 (ADDRHI = 0)
    "85 F5"    //   STA $F5                 (digit count = 0)
    "A6 41"    // LOOP: LDX $41             (X = LINEPOS)
    "E4 40"    //   CPX $40                 (LINELEN)
    "B0 24"    //   BCS DONE
    "B5 00"    //   LDA $00,X               (A = LINEBUF[LINEPOS])
    "20 00 C7" //   JSR HEXVAL
    "B0 1D"    //   BCS DONE                (non-hex: stop, don't consume)
    "48"       //   PHA                     (save digit)
    "06 F0"    //   ASL $F0
    "26 F1"    //   ROL $F1
    "06 F0"    //   ASL $F0
    "26 F1"    //   ROL $F1
    "06 F0"    //   ASL $F0
    "26 F1"    //   ROL $F1
    "06 F0"    //   ASL $F0
    "26 F1"    //   ROL $F1                 (ADDR <<= 4)
    "68"       //   PLA                     (restore digit)
    "05 F0"    //   ORA $F0
    "85 F0"    //   STA $F0                 (ADDRLO |= digit)
    "E6 41"    //   INC $41                 (LINEPOS++)
    "E6 F5"    //   INC $F5                 (digit count++)
    "4C 08 C5" //   JMP LOOP
    "A5 F5"    // DONE: LDA $F5
    "F0 02"    //   BEQ FAIL
    "18"       //   CLC
    "60"       //   RTS
    "38"       // FAIL: SEC
    "60";      //   RTS

// PARSE_BYTE ($C600): same shape as PARSE_ADDR but parses into BYTEVAL
// ($F4, 8-bit), capped at 2 digits.
const std::string kParseByteHex =
    "A9 00"    // PARSE_BYTE: LDA #$00
    "85 F4"    //   STA $F4                 (BYTEVAL = 0)
    "85 F5"    //   STA $F5                 (digit count = 0)
    "A5 F5"    // LOOP: LDA $F5
    "C9 02"    //   CMP #$02                (already 2 digits?)
    "B0 22"    //   BCS DONE
    "A6 41"    //   LDX $41                 (X = LINEPOS)
    "E4 40"    //   CPX $40
    "B0 1C"    //   BCS DONE
    "B5 00"    //   LDA $00,X
    "20 00 C7" //   JSR HEXVAL
    "B0 15"    //   BCS DONE                (non-hex: stop, don't consume)
    "48"       //   PHA
    "06 F4"    //   ASL $F4
    "06 F4"    //   ASL $F4
    "06 F4"    //   ASL $F4
    "06 F4"    //   ASL $F4                 (BYTEVAL <<= 4)
    "68"       //   PLA
    "05 F4"    //   ORA $F4
    "85 F4"    //   STA $F4                 (BYTEVAL |= digit)
    "E6 41"    //   INC $41                 (LINEPOS++)
    "E6 F5"    //   INC $F5                 (digit count++)
    "4C 06 C6" //   JMP LOOP
    "A5 F5"    // DONE: LDA $F5
    "F0 02"    //   BEQ FAIL
    "18"       //   CLC
    "60"       //   RTS
    "38"       // FAIL: SEC
    "60";      //   RTS

// DISPATCH ($C800): called with LINEPOS just past a successfully-parsed
// address in ADDR ($F0/$F1). Inspects LINEBUF[LINEPOS] and tail-jumps into
// PEEK/LIST/POKE/RUN (so their own RTS returns straight to DISPATCH's
// caller), or prints '?'+CRLF and returns itself.
const std::string kDispatchHex =
    "A5 41"    // DISPATCH: LDA $41         (LINEPOS)
    "C5 40"    //   CMP $40                 (LINELEN)
    "90 03"    //   BCC NOTEND
    "4C 00 CA" //   JMP PEEK                (end of line -> peek)
    "A6 41"    // NOTEND: LDX $41
    "B5 00"    //   LDA $00,X               (next char)
    "C9 2E"    //   CMP #$2E                ('.')
    "F0 0B"    //   BEQ DOLIST
    "C9 3A"    //   CMP #$3A                (':')
    "F0 31"    //   BEQ DOPOKE
    "C9 20"    //   CMP #$20                (' ')
    "F0 32"    //   BEQ MAYBERUN
    "4C 5B C8" //   JMP ERROR
    "E6 41"    // DOLIST: INC $41           (skip '.')
    "A5 F0"    //   LDA $F0
    "85 F2"    //   STA $F2                 (stash first-addr lo)
    "A5 F1"    //   LDA $F1
    "85 F3"    //   STA $F3                 (stash first-addr hi)
    "20 00 C5" //   JSR PARSE_ADDR          (second address -> $F0/$F1)
    "B0 30"    //   BCS ERROR               (invalid second address)
    "A5 F0"    //   LDA $F0
    "85 F5"    //   STA $F5                 (temp = second addr lo)
    "A5 F1"    //   LDA $F1
    "85 F6"    //   STA $F6                 (temp hi)
    "A5 F2"    //   LDA $F2
    "85 F0"    //   STA $F0                 ($F0/$F1 = first addr = START)
    "A5 F3"    //   LDA $F3
    "85 F1"    //   STA $F1
    "A5 F5"    //   LDA $F5
    "85 F2"    //   STA $F2                 ($F2/$F3 = second addr = END)
    "A5 F6"    //   LDA $F6
    "85 F3"    //   STA $F3
    "4C 00 CB" //   JMP LIST
    "E6 41"    // DOPOKE: INC $41           (skip ':')
    "4C 00 CC" //   JMP POKE
    "A6 41"    // MAYBERUN: LDX $41
    "E8"       //   INX                     (skip the space)
    "E4 40"    //   CPX $40
    "B0 09"    //   BCS ERROR               (nothing after space)
    "B5 00"    //   LDA $00,X
    "C9 52"    //   CMP #$52                ('R')
    "D0 03"    //   BNE ERROR
    "4C 00 CD" //   JMP RUN
    "A9 3F"    // ERROR: LDA #$3F           ('?')
    "20 00 C1" //   JSR PUTCHAR
    "A9 0D"    //   LDA #$0D
    "20 00 C1" //   JSR PUTCHAR
    "A9 0A"    //   LDA #$0A
    "20 00 C1" //   JSR PUTCHAR
    "60";      //   RTS

// PEEK ($CA00): prints ADDR ($F0/$F1) as a 4-digit hex address, ": ", the
// byte at that address as 2 hex digits, then CRLF.
const std::string kPeekHex =
    "A5 F1"    // PEEK: LDA $F1             (addr hi)
    "20 00 C2" //   JSR PRINT_HEX_BYTE
    "A5 F0"    //   LDA $F0                 (addr lo)
    "20 00 C2" //   JSR PRINT_HEX_BYTE
    "A9 3A"    //   LDA #$3A                (':')
    "20 00 C1" //   JSR PUTCHAR
    "A9 20"    //   LDA #$20                (' ')
    "20 00 C1" //   JSR PUTCHAR
    "A0 00"    //   LDY #$00
    "B1 F0"    //   LDA ($F0),Y             (byte at ADDR)
    "20 00 C2" //   JSR PRINT_HEX_BYTE
    "A9 0D"    //   LDA #$0D
    "20 00 C1" //   JSR PUTCHAR
    "A9 0A"    //   LDA #$0A
    "20 00 C1" //   JSR PUTCHAR
    "60";      //   RTS

// LIST ($CB00): prints START ($F0/$F1) through END ($F2/$F3) inclusive,
// 16 bytes per row, each row prefixed with its own start address. The
// newline for a full row is printed eagerly (before the next row's
// prefix, not after the 16th byte) so a range that stops exactly on a row
// boundary doesn't get a stray blank prefix or a doubled CRLF at STOP.
const std::string kListHex =
    "A9 00"    // LIST: LDA #$00
    "85 F7"    //   STA $F7                 (ROWCOUNT = 0)
    "A5 F1"    // LOOP: LDA $F1             (START hi)
    "C5 F3"    //   CMP $F3                 (END hi)
    "90 0A"    //   BCC CONTINUE
    "D0 4B"    //   BNE STOP
    "A5 F0"    //   LDA $F0                 (START lo)
    "C5 F2"    //   CMP $F2                 (END lo)
    "F0 02"    //   BEQ CONTINUE
    "B0 43"    //   BCS STOP
    "A5 F7"    // CONTINUE: LDA $F7
    "D0 14"    //   BNE SKIP_PREFIX
    "A5 F1"    //   LDA $F1
    "20 00 C2" //   JSR PRINT_HEX_BYTE
    "A5 F0"    //   LDA $F0
    "20 00 C2" //   JSR PRINT_HEX_BYTE
    "A9 3A"    //   LDA #$3A                (':')
    "20 00 C1" //   JSR PUTCHAR
    "A9 20"    //   LDA #$20                (' ')
    "20 00 C1" //   JSR PUTCHAR
    "A0 00"    // SKIP_PREFIX: LDY #$00
    "B1 F0"    //   LDA ($F0),Y
    "20 00 C2" //   JSR PRINT_HEX_BYTE
    "A9 20"    //   LDA #$20                (' ')
    "20 00 C1" //   JSR PUTCHAR
    "E6 F0"    //   INC $F0
    "D0 02"    //   BNE NOCARRY
    "E6 F1"    //   INC $F1
    "E6 F7"    // NOCARRY: INC $F7
    "A5 F7"    //   LDA $F7
    "C9 10"    //   CMP #$10
    "90 BE"    //   BCC LOOP
    "A9 0D"    //   LDA #$0D
    "20 00 C1" //   JSR PUTCHAR
    "A9 0A"    //   LDA #$0A
    "20 00 C1" //   JSR PUTCHAR
    "A9 00"    //   LDA #$00
    "85 F7"    //   STA $F7                 (ROWCOUNT = 0)
    "4C 04 CB" //   JMP LOOP
    "A5 F7"    // STOP: LDA $F7
    "F0 0A"    //   BEQ DONE_NOCRLF
    "A9 0D"    //   LDA #$0D
    "20 00 C1" //   JSR PUTCHAR
    "A9 0A"    //   LDA #$0A
    "20 00 C1" //   JSR PUTCHAR
    "60";      // DONE_NOCRLF: RTS

// POKE ($CC00): entered with LINEPOS just past ':'. Skips spaces, parses
// one hex byte token at a time via PARSE_BYTE, writes each to consecutive
// addresses starting at ADDR ($F0/$F1), stops at end of line or the first
// invalid token.
const std::string kPokeHex =
    "A5 41"    // POKE/LOOP: LDA $41
    "C5 40"    //   CMP $40
    "B0 21"    //   BCS DONE
    "A6 41"    //   LDX $41
    "B5 00"    //   LDA $00,X
    "C9 20"    //   CMP #$20                (space?)
    "D0 05"    //   BNE PARSEBYTE
    "E6 41"    //   INC $41                 (skip space)
    "4C 00 CC" //   JMP LOOP
    "20 00 C6" // PARSEBYTE: JSR PARSE_BYTE
    "B0 0F"    //   BCS DONE                (invalid -> stop)
    "A0 00"    //   LDY #$00
    "A5 F4"    //   LDA $F4
    "91 F0"    //   STA ($F0),Y             (write byte at ADDR)
    "E6 F0"    //   INC $F0
    "D0 02"    //   BNE NOCARRY
    "E6 F1"    //   INC $F1
    "4C 00 CC" // NOCARRY: JMP LOOP
    "60";      // DONE: RTS

// RUN ($CD00): ROM::write() is a no-op, so the firmware can't
// self-modify a JSR operand in place. Instead it builds a 4-byte
// trampoline in RAM at $F8-$FB ("JSR <addr>" + "RTS") and JSRs to that.
// A user-program RTS returns into the trampoline's own RTS, which returns
// to DISPATCH's caller.
const std::string kRunHex =
    "A9 20"    // RUN: LDA #$20             (JSR opcode)
    "85 F8"    //   STA $F8
    "A5 F0"    //   LDA $F0
    "85 F9"    //   STA $F9
    "A5 F1"    //   LDA $F1
    "85 FA"    //   STA $FA
    "A9 60"    //   LDA #$60                (RTS opcode)
    "85 FB"    //   STA $FB
    "20 F8 00" //   JSR $00F8               (execute trampoline)
    "60";      //   RTS

} // namespace monitor
