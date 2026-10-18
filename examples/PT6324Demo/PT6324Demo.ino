/**
 * Example for the PT6324 VFD controller library.
 *
 * Wiring (CDJ-100 drop-in board, Raspberry Pi Pico 2):
 *   PT6324 STB  -> GP1 (level shifted to 5 V)
 *   PT6324 CLK  -> GP2
 *   PT6324 DIN  -> GP3
 *   PT6324 DOUT -> GP4 (optional, only for key scan; pull up to VDD)
 *
 * The VFD itself is 11 grids x 12 segments. Grids 1..11 carry the
 * display content; grids 12..16 are wired back to the Pico (G12..G16)
 * for button-scan synchronisation. The button groups only use segments
 * S1..S5 (bit0..bit4 of byte 0), which is why the lower byte carries a
 * single bit per group and the upper two bytes are zero.
 */

#include <Arduino.h>
#include <PT6324.h>

// Pin assignments (adjust for your hardware)
static const PT6324::Pin PT6324_STROBE = 1;
static const PT6324::Pin PT6324_SCK    = 2;
static const PT6324::Pin PT6324_MOSI   = 3;
static const PT6324::Pin PT6324_MISO   = NOPIN;  // connect DOUT for key scan

PT6324 vfd(PT6324_STROBE, PT6324_SCK, PT6324_MOSI, PT6324_MISO);

/**
 * The 48-byte Display RAM image:
 *   byte 0 : segments SG1..SG8
 *   byte 1 : segments SG9..SG16
 *   byte 2 : segments SG17..SG24
 *   bytes 3n, 3n+1, 3n+2 belong to digit n+1 (DIG1..DIG16)
 *
 * Digits 12..16 (bytes 33..47) are the button synchronisation groups:
 * each carries a single segment line (S1..S5), all other segments off.
 */
void buildButtonGroups(uint8_t* frame) {
    frame[34] = 0x01;  // G12: S1
    frame[37] = 0x02;  // G13: S2
    frame[40] = 0x04;  // G14: S3
    frame[43] = 0x08;  // G15: S4
    frame[46] = 0x10;  // G16: S5
}

void setup() {
    Serial.begin(115200);
    while (!Serial) {
        delay(10);
    }

    vfd.begin();
    vfd.setDimming(7);  // 14/16 pulse width, brightest

    uint8_t frame[PT6324::RAM_BYTES];
    memset(frame, 0x00, sizeof(frame));
    frame[0] = 0xC0;  // top frame line (S1..S5, S9..S12)
    buildButtonGroups(frame);
    vfd.writeData(0, frame, sizeof(frame));

    Serial.println("PT6324 ready");
}

uint8_t keyBuffer[PT6324::MAX_KEY_BYTES];

void loop() {
    // Example: read the key scan result every second
    static unsigned long last = 0;
    if (millis() - last >= 1000) {
        last = millis();
        if (vfd.readKeys(keyBuffer, sizeof(keyBuffer))) {
            Serial.printf("Keys: %02x %02x %02x %02x\n",
                          keyBuffer[0], keyBuffer[1],
                          keyBuffer[2], keyBuffer[3]);
        }
    }
}
