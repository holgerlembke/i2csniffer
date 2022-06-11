const uint8_t PinSCLSpy = 17;
const uint8_t PinSDASpy = 16;

/*
   Buffer-Struktur:
     [low len][high len][adress+mode][data]...

   Buffer handling is a bit on the not so easy side,
   because the [low][high] pair could by sitting at ring buffer end...


  RingWritePtr: 343 RingReadPtr: 319 Doooh. Reset!
  733 27 FD
  737 4 2 0 40 F8 2 0
  746 5 0 4E 0 1 2 3 4 5 6 7 8 9 2 0 40 FA 2 0 4E FA B 0 4E 0 1 2 3 4 5 6 7 8 9 2 0 40 FB 2 0 4E F8 B 0 4E 0 1 2 3 4 5 6 7 8 9 2 0 40 FF 2 0 4E FA B 0 4E 0 1 2 3 4 5 6 7 8 9 2 0
  826 1 0 4E FE B 0 4E 0 1 2 3 4 5 6 7 8 9 2 0 40 FC 2 0 4E FF B 0 4E 0 1 2 3 4 5 6 7 8 9 2 0 40 F8 2 0 4E FB 0 4E 0 1 2 3 4 5 6 7 8 9 2 0 40 F8 2 0
  892 5 0 4E 0 1 2 3 4 5 6 7 8 9 2 0 40 FC 2 0 4E FF B 0 4E 0 1 2 3 4 5 6 7 8 9 2 0 40 F8 2 0 4E FB B 0 4E 0 1 2 3 4 5 6 7 8 9 2 0 40 F9 2 0 4E FA B 0 4E 0 1 2 3 4 5 6 7 8 9 2 0
  972 1 0 4E F8 B 0 4E 0 1 2 3 4 5 6 7 8 9 2 0 40 FF 2 0 4E FC B 0 4E 0 1 2 3 4 5 6 7 8 9 2 0 40 FE 2 0 4E FD B 0 4E 0 1 2 3 4 5 6 7 8 9 2 0 40 FF 2
  14 27 FE
  18 27 0 1 2 3 4 5 6 7 8 9
  31 20 FA


  514 27 0 1 2 3 4 5 6 7 8 9
  527 20 F8
  531 27 FE
  535 27 0 1 2 3 4 5 6 7 18 9
  548 20 F9
  552 27 FA
  556 27 0 1 2 3 4 5 6 7 8 9
  569 20 F8
  573 27 FE
  577 27 0 1 2 3 4 5 6 7 8 9


  968 27 FA
  972 27 0 1 2 3 4 5 6 7 8 9
  985 20 FE
  989 27 F8
  993 27 0 1 2 3 4 5 6 7 8 9
  1006 20
  1009 27 F9
  1013 27 0 1 2 3 4 5 6 7 8 9
  2 20 FE
  6 27 FD
  10 27 0 1 2 3 4 5 6 7 8 9
  23 20 FA


*/

const int16_t RingSize = 1024;
uint8_t ring[RingSize];

int16_t RingWritePtr = 0;
int16_t RingWriteHeaderPtr = 0;   // ptr to len-header, written at stop condition
int16_t RingReadPtr = 0;

// Receiving bit, building bytes, counting packet len
volatile uint8_t RingBitBuffer = 0;
volatile int8_t RingBitCntr = 0;
volatile int16_t RingCounter = 0;    // counts the byte of a transmission

enum i2cstates_t { i2cnone, i2cstartbitreceived};

volatile i2cstates_t i2cstate = i2cnone;
volatile byte lock = 0;

//************************************************************************************
// better: ESP.getCycleCount()
uint32_t IRAM_ATTR getClockCount()
{
  uint32_t ccount;
  __asm__ __volatile__("rsr %0,ccount":"=a" (ccount));

  return ccount;
}

//************************************************************************************
void IRAM_ATTR incrementator(int16_t &v) { // advance ring buffer ptr with end management
  v = v == (RingSize - 1) ? 0 : v + 1;
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
void IRAM_ATTR SCLIntr() // RISING
{
  // waste some time... does it help?
  // wikipedia: Wait for SDA value to be written by target, minimum of 4us for standard mode
  /** /
  uint32_t cs = ESP.getCycleCount();
  int cyclius = 1000 / ESP.getCpuFreqMHz();  // 240 -> in MHz, 4us
  do {
    __asm__ __volatile__ ("nop");
  } while (cs - ESP.getCycleCount() < 130);
  /**/

  bool newStateSDA = digitalRead(PinSDASpy);

  if ( (i2cstate == i2cstartbitreceived) ) {
    if (RingBitCntr < 8) { // Data
      RingBitBuffer = (RingBitBuffer << 1) + (newStateSDA);
      RingBitCntr++;

      if (RingBitCntr == 8) {
        ring[RingWritePtr] = RingBitBuffer;
        incrementator(RingWritePtr);
        RingCounter++;
        RingBitBuffer = 0;
      }
    } else { // 9. bit: ACK/NACK
      RingBitCntr = 0;
    }
  }
}

//************************************************************************************
void IRAM_ATTR SDAIntr() { // CHANGE
  bool newStateSCL = digitalRead(PinSCLSpy);

  if (newStateSCL) { // SCL is high
    bool newStateSDA = digitalRead(PinSDASpy);
    if (newStateSDA) { // rising
      if ( (i2cstate != i2cnone) ) { // a stop condition
        i2cstate = i2cnone;

        // save the packet len
        lock++;
        ring[RingWriteHeaderPtr] = RingCounter & 0xff;
        incrementator(RingWriteHeaderPtr);
        ring[RingWriteHeaderPtr] = RingCounter >> 8;
        lock--;
      }
    } else { // falling
      if ( (i2cstate == i2cnone) ) { // a start condition
        RingWriteHeaderPtr = RingWritePtr;
        ring[RingWritePtr] = 0;
        incrementator(RingWritePtr);
        ring[RingWritePtr] = 0;
        incrementator(RingWritePtr);
        RingBitCntr = 0;
        RingCounter = 0;
        RingBitBuffer = 0;

        i2cstate = i2cstartbitreceived;
      }
    }
  }
}

//************************************************************************************
void i2cSpybegin() {
  pinMode(PinSCLSpy, INPUT);
  pinMode(PinSDASpy, INPUT);

  attachInterrupt(PinSDASpy, SDAIntr, CHANGE);
  attachInterrupt(PinSCLSpy, SCLIntr, RISING);

  i2cstate = i2cnone;
  RingWritePtr = 0;
  RingReadPtr = 0;
}

//************************************************************************************
void i2cSpyend() {
  detachInterrupt(PinSDASpy);
  detachInterrupt(PinSCLSpy);
}

//************************************************************************************
void setupI2CSpyAgent(void) {
  i2cSpybegin();
}

//************************************************************************************
void loopI2CSpyAgent(void) {
  static unsigned long ticker = 0;

  if (millis() - ticker > 4000) {
    ticker = millis();

    int len = peekRingDataLen();
    while ( (RingReadPtr != RingWritePtr) && (len > 0) ) {
      incrementator(RingReadPtr);
      incrementator(RingReadPtr);
      // Limiter
      if (len > 200) {
        Serial.print("\nRingWritePtr: ");
        Serial.print(RingWritePtr);
        Serial.print(" RingReadPtr: ");
        Serial.print(RingReadPtr);
        i2cSpyend();
        i2cSpybegin();
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
