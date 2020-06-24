#include <OneWire.h>
#include <DallasTemperature.h>


OneWire ds(13);
DallasTemperature sensors(&ds);
DeviceAddress insideThermometer;

bool tempsens_found = false;
float tempsens_value = 0;


void tempsens_setup()
{
  sensors.begin();

  if(sensors.getDeviceCount() == 0)
  {
    return;
  }
  Serial.print("Found ");
  Serial.print(sensors.getDeviceCount(), DEC);
  Serial.println(" devices.");

  
  Serial.print("Parasite power is: "); 
  if (sensors.isParasitePowerMode()) Serial.println("ON");
  else Serial.println("OFF");

  
  if (!sensors.getAddress(insideThermometer, 0))
  {
    Serial.println("Unable to find address for Device 0"); 
    return;
  }

  sensors.setResolution(insideThermometer, 9);
 
  Serial.print("Device 0 Resolution: ");
  Serial.print(sensors.getResolution(insideThermometer), DEC); 
  Serial.println();

  tempsens_found = true;
  Serial.printf("[Sensors] Found 1-Wire sensor: %02X%02X%02X%02X%02X%02X%02X%02X\n", insideThermometer[0], insideThermometer[1], insideThermometer[2], insideThermometer[3], insideThermometer[4], insideThermometer[5], insideThermometer[6], insideThermometer[7]);
}



bool tempsens_loop()
{
  int curTime = millis();
  static int nextTime = 0;

  if(nextTime > curTime)
  {
    return false;
  }
  nextTime = curTime + 1000;

  if(!tempsens_found)
  {
    tempsens_setup();
    return false;
  }
  
  sensors.requestTemperatures(); // Send the command to get temperatures
  tempsens_value = sensors.getTempC(insideThermometer);
  //Serial.printf("[Sensors] temp %f\n", tempsens_value);

  return false;
}

