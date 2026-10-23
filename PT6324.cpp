/**
 * @file    PT6324.cpp
 * @brief   Implementation of the Princeton Technology PT6324
 *          VFD Driver/Controller driver for Arduino.
 *
 * Copyright (c) 2026 Florian Heller
 * MIT License
 */

#include "PT6324.h"
#include <SPI.h>


// We need this because the encoding of the number of digits doesn't follow common conventions
const uint8_t digitsMask[9] = { 0x0, 0x8, 0x9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF };

// Minimum timing constraints (datasheet, timing characteristics):
//   PW_STB (strobe pulse width)   >= 1000 ns
//   twait (command byte to first
//   data clock edge)              >= 1000 ns
//   CLK pulse width               >= 400 ns (max ~2.5 MHz)
//
// STB is wired as the SPI chip-select pin, so it is driven by the SPI
// core inside beginTransaction()/endTransaction(). The STB goes high at
// the end of a transaction; the STB_PULSE_US delay guarantees the
// minimum STB pulse width between consecutive frames.
static constexpr uint32_t STB_PULSE_US = 2;   // STB high time after a frame
static constexpr uint32_t WAIT_TIME_US = 3;   // twait between command and data

PT6324::PT6324(Pin strobe, Pin sck, Pin mosi, Pin miso, uint32_t clock)
    : _strobe(strobe), _sck(sck), _mosi(mosi), _miso(miso) {
    _settings = SPISettings(clock, LSBFIRST, SPI_MODE3);
}

void PT6324::begin() {
    if (_miso != NOPIN) {
        SPI.setRX(_miso);
    }
    SPI.setSCK(_sck);
    SPI.setTX(_mosi);
    SPI.setCS(_strobe);

    SPI.begin(true); // True means we're using hardware controlled CS.

    // Power-on flow (see datasheet "Recommended Software Flowchart"):
    // 1. clear the Display RAM (undefined at power on)
    // 2. set the display mode
    // 3. turn the display on
    //
    // Order matters: a mode-setting command (Command 1) forces the
    // display OFF, so it has to be executed before the display-on
    // command (Command 4).
    clear();
    setDigits(MAX_DIGITS);
    setDisplay(true);
}

// ---------------------------------------------------------------------------
// Command 1: Display Mode Setting
// ---------------------------------------------------------------------------

bool PT6324::setDigits(uint8_t digits) {
    if (digits < MIN_DIGITS || digits > MAX_DIGITS) {
        return false;
    }
    _digits = digits;

    // Command 1: 0000 b3 b2 b1 b0, 0000 = 8 digits, 1111 = 16 digits
    const uint8_t cmd = digitsMask[digits - MIN_DIGITS];
    _beginFrame()
    SPI.transfer(cmd);
    _endFrame()
    return true;
}

// ---------------------------------------------------------------------------
// Command 2 + 3: Data Setting / Address Setting
// ---------------------------------------------------------------------------

bool PT6324::writeData(uint8_t address, const uint8_t* data, size_t len) {
    if (address > RAM_MAX_ADDR || len == 0 || address + len > RAM_BYTES) {
        return false;
    }
    _beginFrame()
    _sendByte(CMD_WRITE_DATA);           // Command 2: write, address increment
    _sendByte(CMD_ADDR_BASE | address);  // Command 3: start address
    SPI.transfer(const_cast<uint8_t*>(data), len);
    _endFrame()
    return true;
}

bool PT6324::writeByte(uint8_t address, uint8_t data) {
    if (address > RAM_MAX_ADDR) {
        return false;
    }

    _beginFrame()
    _sendByte(CMD_WRITE_FIXED);     // Command 2: write, fixed address
    _sendByte(CMD_ADDR_BASE | address);  // Command 3: address
    _sendByte(data);
    _endFrame()
    return true;
}

bool PT6324::clear(uint8_t address, size_t len) {
    if (address > RAM_MAX_ADDR || len == 0 || address + len > RAM_BYTES) {
        return false;
    }
    static const uint8_t zeros[RAM_BYTES] = {0};
    return writeData(address, zeros, len);
}

// ---------------------------------------------------------------------------
// Digit level convenience API
// ---------------------------------------------------------------------------

bool PT6324::setDigit(uint8_t digit, uint8_t seg1_8, uint8_t seg9_16, uint8_t seg17_24) {
    if (digit >= MAX_DIGITS) {
        return false;
    }
    const uint8_t bytes[DIGIT_BYTES] = {seg1_8, seg9_16, seg17_24};
    return writeData(digit * DIGIT_BYTES, bytes, DIGIT_BYTES);
}

// ---------------------------------------------------------------------------
// Command 4: Display Control
// ---------------------------------------------------------------------------

void PT6324::setDisplay(bool on) {
    setDisplay(on, _dimming);
}

void PT6324::setDimming(uint8_t level) {
    _dimming = level & 0x07;
    _beginFrame()
    // Display stays on (b3 = 1) while the dimming level changes
    _sendByte(CMD_DISPLAY_BASE | CMD_DISPLAY_ON | _dimming);
    _endFrame()
}

void PT6324::setDisplay(bool on, uint8_t dimming) {
    _dimming = dimming & 0x07;
    const uint8_t cmd = CMD_DISPLAY_BASE | (_dimming & 0x07)
                       | (on ? CMD_DISPLAY_ON : 0x00);
    _beginFrame()
    _sendByte(cmd);
    _endFrame()
}

// ---------------------------------------------------------------------------
// Reset
// ---------------------------------------------------------------------------

void PT6324::reset() {
    _beginFrame()
    _sendByte(CMD_RESET);  // Command 2, reset mode: one-time reset
    _endFrame()
}

// ---------------------------------------------------------------------------
// Key scan
// ---------------------------------------------------------------------------

bool PT6324::readKeys(uint8_t* data, size_t len) {
    if (_miso == NOPIN || data == nullptr || len == 0 || len > MAX_KEY_BYTES) {
        return false;
    }

    _beginFrame()
    _sendByte(CMD_READ_KEY);
    for (size_t i = 0; i < len; i++) {
        data[i] = SPI.transfer(0x00);
    }
    _endFrame()
    return true;
}

// ---------------------------------------------------------------------------
// Low-level transfer primitives
// ---------------------------------------------------------------------------

void PT6324::_beginFrame() {
    // STB goes low (chip select asserted)
    SPI.beginTransaction(_settings);
}

void PT6324::_endFrame() {
    // STB goes high (rising edge latches the frame)
    SPI.endTransaction();
    delayMicroseconds(STB_PULSE_US);
}

void PT6324::_sendByte(uint8_t byte) {
    SPI.transfer(byte);
    delayMicroseconds(WAIT_TIME_US);
}

uint8_t PT6324::_readByte() {
    delayMicroseconds(WAIT_TIME_US);
    return SPI.transfer(0x00);