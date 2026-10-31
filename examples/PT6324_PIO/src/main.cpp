#include "main.h"
#include <PT6324.h>

PT6324 displayController = PT6324(PT6324_CS, PT6324_SCK, PT6324_TX, PT6324_RX, 100000);

const long interval = 1000;
unsigned long lastUpdate = 0;

// Running on Core 0



void setup() {
 
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  delay(100);

  displayController.begin();

  delay(100);
  
  Serial.println("Setup complete");

}


void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - lastUpdate >= interval) {
        uint8_t value = (state==true) ? 0xa5 : 0x5a;
        unsigned char msg[48];
        memset(msg, value, sizeof(msg));
        displayController.writeData(0, msg, sizeof(msg));  
        digitalWrite(LED_BUILTIN, state);
        state = !state;
        lastUpdate = currentMillis;
    }
}