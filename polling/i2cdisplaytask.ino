//************************************************************************************
void displaytaskSetup() {
  xTaskCreatePinnedToCore(
    i2cDisplayTask,
    "disp",
    4000,             // Stack size
    NULL,
    1,                // Priority
    NULL,
    0);               // Core 0 cooperative!
}

//************************************************************************************
int16_t peekRingDataLen() {
  if (lock > 0) {
    return 0;
  }

  int16_t tmp = RingReadPtr;

  int16_t len = ring[tmp];
  incrementator(tmp);

  return (len + (ring[tmp] >> 8));
}

//************************************************************************************
void i2cDisplayTask(void * parameter) {
  while (1) {
    delay(1); // feed the dog

    static unsigned long ticker = 0;

    if (millis() - ticker > 4000) {
      ticker = millis();

      int len = peekRingDataLen();
      if (len==0) {
        Serial.println("No data!");
      }
      while ( (RingReadPtr != RingWritePtr) && (len > 0) ) {
        incrementator(RingReadPtr);
        incrementator(RingReadPtr);
        // Limiter
        if (len > 200) {
          Serial.print("\nRingWritePtr: ");
          Serial.print(RingWritePtr);
          Serial.print(" RingReadPtr: ");
          Serial.print(RingReadPtr);
          Serial.println(" Doooh. Reset!");
        } else {
          Serial.print(RingReadPtr);
          Serial.print(" ");
          bool first = true;
          while (len > 0) {
            // first byte has R/W in lowest bit
            if (first) {
              Serial.print(ring[RingReadPtr] >> 1, HEX);
              first = false;
            } else {
              Serial.print(ring[RingReadPtr], HEX);
            }
            incrementator(RingReadPtr);
            len--;
            Serial.print(" ");
          }
          Serial.println();
        }
        len = peekRingDataLen();
      }
    }
  }
}
