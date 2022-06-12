#include <Wire.h>

// erzeugt i2c-datenverkehr und lustige muster
// erwartet PCF 8574, led an p0, p1, p2
// I2c-Bus-Standardkonfiguration (Wemos d1 mini, scl = d1/GPIO5, SDA = d2/GPIO4)
// https://www.youtube.com/watch?v=NuN65ICbxwk

byte dev1 = 0, dev2 = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("i2c spy source - data creator");

  Wire.begin();

  for (uint8_t i = 10; i < 160; i++) {
    Wire.beginTransmission(i);
    int erg = Wire.endTransmission();
    if (erg == 0) {
      if (dev1 == 0) {
        dev1 = i;
      } else {
        dev2 = i;
      }
      Serial.print("Found device at 0x");
      Serial.print(i, HEX);
      Serial.println();
    }
  }

  srand(micros());
}

byte bit1 = 0xff, bit2 = 0xff;

void loop() {
  int result;

  // Pseudo-Dev, 0..9 senden
  Wire.beginTransmission(dev2);
  for (int i = 0; i < 10; i++) {
    Wire.write(i);
  }
  result = Wire.endTransmission();

  // dev1
  Wire.beginTransmission(dev1);
  Wire.write(bit1);
  result = Wire.endTransmission();
  if (result != 0) {
    Serial.println("Panic!");
  }
  bit1 = bit1 ^ (1 << random(0, 3));

  // dev2
  Wire.beginTransmission(dev2);
  Wire.write(bit2);
  result = Wire.endTransmission();
  if (result != 0) {
    Serial.println("Panic!");
  }
  bit2 = bit2 ^ (1 << random(0, 3));



  delay(400);
}
