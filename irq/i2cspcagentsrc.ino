/*
  esp32 interrupt latency

  https://esp32.com/viewtopic.php?t=422
  I have done a measurement and delay from external trigger to application-provided ISR handler 
  is around 2us (at 240MHz clock), which is around 500 cycles.
 
  https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/hlinterrupts.html
 
*/

const uint8_t PinSCLSpy = 17;
const uint8_t PinSDASpy = 16;

/*
   Buffer-Struktur:
     [low len][high len][adress+mode][data]...

   Buffer handling is a bit on the not so easy side,
   because the [low][high] pair could by sitting at ring buffer end...

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
void IRAM_ATTR incrementatorx(int16_t &v) { // advance ring buffer ptr with end management
  v = v == (RingSize - 1) ? 0 : v + 1;
}

//************************************************************************************
#define incrementator(v) v = v == (RingSize - 1) ? 0 : v + 1;

#pragma GCC push_options
#pragma GCC optimize ("O2")
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

// waste some time... does it help?
// wikipedia: Wait for SDA value to be written by target, minimum of 4us for standard mode
// Code in progress, not really thought out...
/** /
  uint32_t cs = ESP.getCycleCount();
  int cyclius = 1000 / ESP.getCpuFreqMHz();  // 240 -> in MHz, 4us
  do {
  __asm__ __volatile__ ("nop");
  } while (cs - ESP.getCycleCount() < 130);
  /**/


//************************************************************************************
void IRAM_ATTR SCLIntr() // RISING
{
  if ( (i2cstate == i2cstartbitreceived) ) {
    // Saving a call
    bool newStateSDA = gpio_get_level((gpio_num_t)PinSDASpy); // digitalRead(PinSDASpy);

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
  bool newStateSCL = gpio_get_level((gpio_num_t)PinSCLSpy); // digitalRead(PinSCLSpy);

  if (newStateSCL) { // SCL is high
    bool newStateSDA = gpio_get_level((gpio_num_t)PinSDASpy); // digitalRead(PinSDASpy);

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
#pragma GCC pop_options

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

  /** /
    int t = 1023;
    incrementator(t);
    Serial.println(t);
    t=42;
    incrementator(t);
    Serial.println(t);
    /**/
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
