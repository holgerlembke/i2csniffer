
// Target platform: esp32 wemos minikit
// i2c-monitor: scl=GPIO17, sda=GPIO16

/*

Found device at 0x20
Found device at 0x27

 */


void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("i2c spy agent - spy the i2c traffic");
  Serial.print("CPU: ");
  Serial.println(ESP.getCpuFreqMHz());

  setupI2CSpyAgent();
}

void loop() {
  loopI2CSpyAgent();
}
