const uint8_t PinSCLSpy = 17;
const uint8_t PinSDASpy = 16;

// Data structures

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
