#include <Wire.h>
#include "MAX30105.h"

MAX30105 particleSensor;

void setup()
{
  Serial.begin(115200);
  Wire.begin(21, 22);

  if (!particleSensor.begin(Wire, I2C_SPEED_FAST))
  {
    Serial.println("MAX30102 not found");
    while (1);
  }

  particleSensor.setup();
}

void loop()
{
  long irValue = particleSensor.getIR();
  Serial.print("IR Value: ");
  Serial.println(irValue);
  delay(500);
}
