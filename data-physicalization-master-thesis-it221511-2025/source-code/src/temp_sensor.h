#ifndef TEMP_SENSOR_H
#define TEMP_SENSOR_H

#include <Wire.h>
#include <Adafruit_HTU21DF.h>

class TempSensor
{
private:
	Adafruit_HTU21DF sht21 = Adafruit_HTU21DF();
	unsigned long lastRequestTime = 0;
	;
	bool sensorAvailable = false;

public:
	// Initialize the sensor with custom I2C pins
	void initTempSensor(uint8_t sda = 9, uint8_t scl = 10)
	{
		Wire.begin(9, 10);
		delay(100); // Allow I2C bus to stabilize
		if (sht21.begin())
		{
			sensorAvailable = true;
			Serial.println("Sensor available");
			float currentTemperature = sht21.readTemperature();
			Serial.print("Current temperature: ");
			Serial.println(currentTemperature);
		}else{
			Serial.println("Sensor not available");
		}
	}

	// Print temperature every 10 seconds without delay
	void checkTemperature()
	{

		if (millis() - lastRequestTime >= 600000)
		{
			lastRequestTime = millis();
			if (!sensorAvailable)
			{
				return;
			}

			float temperature = sht21.readTemperature();
			sendTemperature(temperature);
		}
	}
};

#endif // TEMP_SENSOR_H