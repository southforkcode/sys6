#pragma once

#include "memory/rom.h"

#include <cstdint>
#include <string>

namespace monitor {

constexpr uint16_t kRomBase = 0xC000;
constexpr uint16_t kRomSize = 0x4000;

// Zero-page layout
constexpr uint16_t kLineBufAddr = 0x0000; // 64 bytes, $00-$3F
constexpr uint16_t kLineLenAddr = 0x0040;
constexpr uint16_t kLinePosAddr = 0x0041;
constexpr uint16_t kAddrLoAddr = 0x00F0;
constexpr uint16_t kAddrHiAddr = 0x00F1;
constexpr uint16_t kAddr2LoAddr = 0x00F2;
constexpr uint16_t kAddr2HiAddr = 0x00F3;
constexpr uint16_t kByteValAddr = 0x00F4;
constexpr uint16_t kScratchLoAddr = 0x00F5;
constexpr uint16_t kScratchHiAddr = 0x00F6;
constexpr uint16_t kRowCountAddr = 0x00F7;
constexpr uint16_t kRunTrampolineAddr = 0x00F8; // 4 bytes, $F8-$FB
constexpr uint16_t kStrPtrLoAddr = 0x00FC;
constexpr uint16_t kStrPtrHiAddr = 0x00FD;

// ROM routine addresses (bus addresses -- see docs/superpowers/plans for the
// full table and why each routine gets a fixed 256-byte page)
constexpr uint16_t kGetCharAddr = 0xC000;
constexpr uint16_t kPutCharAddr = 0xC100;
constexpr uint16_t kPrintHexByteAddr = 0xC200;
constexpr uint16_t kPrintNibbleAddr = 0xC220;
constexpr uint16_t kPrintStringAddr = 0xC300;
constexpr uint16_t kHexValAddr = 0xC700;
constexpr uint16_t kParseAddrAddr = 0xC500;
constexpr uint16_t kParseByteAddr = 0xC600;
constexpr uint16_t kDispatchAddr = 0xC800;
constexpr uint16_t kTestDriverAddr = 0xC900; // reserved for tests only
constexpr uint16_t kPeekAddr = 0xCA00;
constexpr uint16_t kListAddr = 0xCB00;
constexpr uint16_t kPokeAddr = 0xCC00;
constexpr uint16_t kRunAddr = 0xCD00;

extern const std::string kGetCharHex;
extern const std::string kPutCharHex;
extern const std::string kPrintHexByteHex;
extern const std::string kPrintNibbleHex;
extern const std::string kPrintStringHex;
extern const std::string kHexValHex;
extern const std::string kParseAddrHex;
extern const std::string kParseByteHex;
extern const std::string kDispatchHex;
extern const std::string kPeekHex;
extern const std::string kListHex;
extern const std::string kPokeHex;
extern const std::string kRunHex;

// Writes `hex` into `rom` at device-relative offset (busAddr - kRomBase),
// via ROM::load() -- not loadProgram()/MemoryDevice::write(), since
// ROM::write() is deliberately a no-op (real ROM semantics: the CPU can't
// write to it) and would silently discard every byte. Bus does the
// bus-address-to-device-offset translation for normal reads/writes, but
// this talks to the ROM directly, so the subtraction happens here too.
void loadRoutine(ROM &rom, uint16_t busAddr, const std::string &hex);

} // namespace monitor
