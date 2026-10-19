/**
 * @file    PT6324.h
 * @brief   Arduino library for the Princeton Technology PT6324
 *          VFD Driver/Controller IC (16 grids x 24 segments).
 *
 * The PT6324 is a Vacuum Fluorescent Display (VFD) controller with a
 * three-line serial interface (STB, CLK, DIN) plus an open-drain data
 * output (DOUT) used for key scanning.
 *
 * Protocol summary (see datasheet PT6324 V1.5, June 2010):
 *   - Data is transmitted LSB first, latched on the rising edge of CLK.
 *   - A byte is accepted by the chip when STB goes HIGH (rising edge).
 *   - While STB is LOW the chip accepts further bytes: the first byte
 *     is a command, subsequent bytes are data.
 *   - Display RAM: 48 bytes (address 00H..2FH), 3 bytes per digit,
 *     16 digits (DIG1..DIG16), 24 segments each.
 *
 * Command set:
 *   Command 1 (Display Mode)        0000 b3 b2 b1 b0 -> (8..16) digits
 *   Command 2 (Data Setting)        0100 b3 b2 b1 b0
 *       b3b2: 00 = write data, 01 = reset, 10 = read key data
 *       b1:   0 = address increment, 1 = fixed address
 *   Command 3 (Address Setting)     11 00 b5 b4 b3 b2 b1 b0 (00H..2FH)
 *   Command 4 (Display Control)     1000 b3 b2 b1 b0
 *       b3:   0 = display off, 1 = display on
 *       b2b1b0: dimming (pulse width 1/16 .. 14/16)
 *
 * Copyright (c) 2026 Florian Heller
 * MIT License
 */

#pragma once
#ifndef PT6324_H
#define PT6324_H

#include <Arduino.h>
#include <SPI.h>

class PT6324 {
public:
    using Pin = pin_size_t;

    // Chip capabilities / limits
    static constexpr uint8_t MIN_DIGITS         = 8;
    static constexpr uint8_t MAX_DIGITS         = 16;
    static constexpr uint8_t SEGMENTS_PER_DIGIT = 24;
    static constexpr uint8_t DIGIT_BYTES        = 3;   // 24 segments = 3 bytes
    static constexpr uint8_t RAM_BYTES          = 48;  // 16 digits * 3 bytes
    static constexpr uint8_t RAM_MAX_ADDR       = 0x2F;
    static constexpr uint8_t MAX_KEY_BYTES      = 4;   // 16x2 key matrix = 32 bits

    // Raw command byte encodings (for advanced use)
    static constexpr uint8_t CMD_WRITE_DATA     = 0x40;  // Cmd 2: write, incrementing address
    static constexpr uint8_t CMD_WRITE_FIXED    = 0x42;  // Cmd 2: write, fixed address
    static constexpr uint8_t CMD_RESET          = 0x41;  // Cmd 2: one-time reset
    static constexpr uint8_t CMD_READ_KEY       = 0x48;  // Cmd 2: read key data
    static constexpr uint8_t CMD_ADDR_BASE      = 0xC0;  // Cmd 3: address = CMD_ADDR_BASE | addr
    static constexpr uint8_t CMD_DISPLAY_BASE   = 0x80;  // Cmd 4: display control base
    static constexpr uint8_t CMD_DISPLAY_ON     = 0x08;  // Cmd 4: display on bit (b3)

    /**
     * @param strobe  Pin connected to the STB pin (chip select, active low)
     * @param sck     Pin connected to the CLK pin
     * @param mosi    Pin connected to the DIN pin
     * @param miso    Pin connected to the DOUT pin (key scan output).
     *                Use NOPIN if key scanning is not used.
     * @param clock   SPI clock in Hz (datasheet max ~2.5 MHz, default 1 MHz)
     */
    PT6324(Pin strobe, Pin sck, Pin mosi, Pin miso = NOPIN, uint32_t clock = 1000000);

    /**
     * Initialise the chip: configure SPI, set 16-digit mode, clear the
     * Display RAM and turn the display on with the default dimming.
     */
    void begin();

    // ------------------------------------------------------------------
    // Command 1: Display Mode Setting
    // ------------------------------------------------------------------

    /**
     * Set the number of digits in use (8..16).
     * Note: executing this command forces the display OFF.
     * A display-on command must be sent afterwards to resume display.
     *
     * @return false if the value is out of range
     */
    bool setDigits(uint8_t digits);

    /**
     * Number of digits configured on this chip.
     */
    uint8_t digits() const { return _digits; }

    // ------------------------------------------------------------------
    // Command 2 + 3: Data Setting / Address Setting
    // ------------------------------------------------------------------

    /**
     * Write a burst of up to 48 bytes to the Display RAM, starting at
     * @p address. The address auto-increments with every byte and wraps
     * at 30H.
     *
     * @param address  Start address 00H..2FH
     * @param data     Pointer to the data bytes
     * @param len      Number of bytes to write,
     *                 1..(48 - address), 48 max
     * @return false if the range is out of bounds
     */
    bool writeData(uint8_t address, const uint8_t* data, size_t len);

    /**
     * Write a single byte to a fixed address (no auto-increment).
     *
     * @return false if the address is out of bounds
     */
    bool writeByte(uint8_t address, uint8_t data);

    /**
     * Clear part of the Display RAM (default: all 48 bytes).
     *
     * @return false if the range is out of bounds
     */
    bool clear(uint8_t address = 0, size_t len = RAM_BYTES);

    // ------------------------------------------------------------------
    // Digit level convenience API
    // ------------------------------------------------------------------

    /**
     * Set all 24 segments of one digit.
     *
     * @param digit   Digit index 0..15 (0 = DIG1)
     * @param seg1_8  Segments SG1..SG8   (bit0 = SG1, ..., bit7 = SG8)
     * @param seg9_16 Segments SG9..SG16  (bit0 = SG9,  ..., bit7 = SG16)
     * @param seg17_24 Segments SG17..SG24 (bit0 = SG17, ..., bit7 = SG24)
     * @return false if the digit index is out of range
     */
    bool setDigit(uint8_t digit, uint8_t seg1_8, uint8_t seg9_16 = 0, uint8_t seg17_24 = 0);

    /**
     * Clear all 24 segments of one digit.
     */
    bool clearDigit(uint8_t digit) {
        return setDigit(digit, 0x00, 0x00, 0x00);
    }

    // ------------------------------------------------------------------
    // Command 4: Display Control
    // ------------------------------------------------------------------

    /**
     * Turn the display on or off (key scanning continues while off).
     */
    void setDisplay(bool on);

    /**
     * Set the dimming level (pulse width of the segment drive).
     *
     * | level | pulse width |
     * |-------|-------------|
     * | 0     | 1/16        |
     * | 1     | 2/16        |
     * | 2     | 4/16        |
     * | 3     | 10/16       |
     * | 4     | 11/16       |
     * | 5     | 12/16       |
     * | 6     | 13/16       |
     * | 7     | 14/16       |
     */
    void setDimming(uint8_t level);

    /**
     * Set display on/off and dimming level in a single command.
     */
    void setDisplay(bool on, uint8_t dimming);

    /**
     * Current dimming level (0..7).
     */
    uint8_t dimming() const { return _dimming; }

    // ------------------------------------------------------------------
    // Reset
    // ------------------------------------------------------------------

    /**
     * Execute a one-time reset (Command 2, reset mode). The chip is
     * returned to its power-on state: 16-digit mode, display off,
     * address 00H, dimming 1/16.
     */
    void reset();

    // ------------------------------------------------------------------
    // Key scan
    // ------------------------------------------------------------------

    /**
     * Read key data from the key scan RAM (16x2 matrix, 32 bits = 4 bytes).
     * Requires the DOUT pin to be connected (miso != NOPIN) and an
     * external pull-up resistor (1k..10k) on DOUT, as per the datasheet.
     *
     * @param data  Buffer that receives the key bytes (LSB first)
     * @param len   Number of bytes to read, 1..4
     * @return false if no DOUT pin was configured
     */
    bool readKeys(uint8_t* data, size_t len);

private:
    Pin _strobe;
    Pin _sck;
    Pin _mosi;
    Pin _miso;

    SPISettings _settings;
    uint8_t _digits = MAX_DIGITS;
    uint8_t _dimming = 3;  // 10/16 pulse width, a sensible default

};

#endif  // PT6324_H
