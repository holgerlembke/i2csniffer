//************************************************************************************
void SnifferTaskSetup() {
  xTaskCreatePinnedToCore(
    i2cSnifferTask,
    "i2cs",
    4000,                      // Stack size
    NULL,
    2,                         // Priority
    NULL,
    1);                        // Core 1, eats all the cpu load on this cpu!
}

#pragma GCC push_options
#pragma GCC optimize ("O2")
//************************************************************************************
void i2cSnifferTask(void * parameter) {
  // We don't want dogs here
  esp_task_wdt_init(30, false);

  Serial.println(F("Start Sniffer on Core 1\n"));
  vTaskDelay(1000);

  pinMode(PinSCLSpy, INPUT);
  pinMode(PinSDASpy, INPUT);

  i2cstate = i2cnone;
  RingWritePtr = 0;
  RingReadPtr = 0;

  bool stateSCL = gpio_get_level((gpio_num_t)PinSCLSpy);
  bool stateSDA = gpio_get_level((gpio_num_t)PinSDASpy);
  while (1) {
    bool newStateSDA = gpio_get_level((gpio_num_t)PinSDASpy);
    /** /
    if (newStateSDA != stateSDA) {
      delayMicroseconds(1);
      newStateSDA = gpio_get_level((gpio_num_t)PinSDASpy);
    }
    /**/
    bool newStateSCL = gpio_get_level((gpio_num_t)PinSCLSpy);
    /** /
    if (newStateSCL != stateSCL) {
      delayMicroseconds(1);
      newStateSCL = gpio_get_level((gpio_num_t)PinSCLSpy);
    }
    /**/

    if ( (stateSCL == LOW) && (newStateSCL == HIGH) ) { // SCL is rising --> Data bit
      // Data bit begin========================================
      if (i2cstate == i2cstartbitreceived) {
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
      // Data bit end==========================================
    }
    if ( (stateSDA != newStateSDA) && (newStateSCL == HIGH) ) { // SDA has changed, SCL high --> start or stop condition
      // Start/Stop begin========================================
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
      // Start/Stop end==========================================
    }

    stateSCL = newStateSCL;
    stateSDA = newStateSDA;
  }
}
#pragma GCC pop_options
