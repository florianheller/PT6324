# PT6324

Arduino library for the [Princeton Technology PT6324](https://www.princeton.com.tw) VFD Driver/Controller IC ([Spec sheet](https://support.newhavendisplay.com/hc/en-us/article_attachments/4415110153623)).

## Chip

- 16 grid x 24 segment VFD controller, 52-pin LQFP
- 48-byte Display RAM (3 bytes per digit, 16 digits)
- Three-line serial interface: **STB** (strobe, active low), **CLK**, **DIN**, plus open-drain **DOUT** for key scan
- 16 x 2 key scan matrix
- 8-step dimming (pulse width 1/16 .. 14/16)
- Data is shifted **LSB first**, latched on the **rising edge of CLK**; a frame is latched by a rising **STB** edge
- VDD 2.7 - 5.5 V, max clock ~2.5 MHz

## Command set
See the [full datasheet](https://www.alldatasheet.com/datasheet-pdf/pdf/311255/PTC/PT6324.html) for more details. 
| Command | Encoding              | Function |
|---------|-----------------------|----------|
| 1       | `0000 bbbbb`          | Display mode: `bbbb` + 8 digits (8..16) |
| 2       | `0100 b3b2 b1 0`      | Data setting: write (00), reset (01), key read (10); `b1` = fixed address |
| 3       | `1100 aaaaaa`         | Set Display RAM address `00H`..`2FH` |
| 4       | `1000 b3 bbb`         | Display control: `b3` on/off, `bbb` dimming |

## Usage

```cpp
#include <PT6324.h>

PT6324 vfd(1, 2, 3, NOPIN);   // STB, CLK, DIN, DOUT
vfd.begin();                   // init, clear RAM, 16 digits, display on

// digit 1 (0-based): SG1..8 = 0xC0
vfd.setDigit(0, 0xC0);

// full-frame write (48 bytes)
uint8_t frame[48] = {0};
vfd.writeData(0, frame, 48);

vfd.setDimming(7);             // 14/16 pulse width
```

See `examples/PT6324Demo` for a complete example including the button
synchronisation groups (grids 12..16) used by the CDJ-100 board.

## Wiring notes

- STB, CLK, DIN are 5 V logic on the CDJ-100 board -> level-shift (TXB0108, no internal pull-ups)
- DOUT (key scan) needs an external pull-up (1 k..10 k to VDD)
- `begin()` runs the datasheet power-on sequence: clear RAM -> set mode -> display on

## License

MIT
