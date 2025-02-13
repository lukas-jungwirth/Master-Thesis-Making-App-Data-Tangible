#ifndef STORE_H
#define STORE_H

#include "SPIFFS.h"

bool initStore()
{
	// Initialize SPIFFS
	if (!SPIFFS.begin(true))
	{
		Serial.println("An Error has occurred while mounting SPIFFS");
		return false;
	}
	return true;
}

String getWifiSSID()
{
	File file = SPIFFS.open("/wifi.json", FILE_READ);
	if (!file)
	{
		Serial.println("Failed to open file for reading");
		return "";
	}

	JsonDocument doc;
	DeserializationError error = deserializeJson(doc, file);

	file.close();

	if (error)
	{
		Serial.println("Failed to parse JSON");
		return "";
	}

	return doc["ssid"].as<String>();
}

String getWifiPassword()
{
	File file = SPIFFS.open("/wifi.json", FILE_READ);
	if (!file)
	{
		Serial.println("Failed to open file for reading");
		return "";
	}

	JsonDocument doc;
	DeserializationError error = deserializeJson(doc, file);

	file.close();

	if (error)
	{
		Serial.println("Failed to parse JSON");
		return "";
	}

	return doc["password"].as<String>();
}

void storeWifiCredentials(String ssid, String password)
{
    // Open the file for writing
    File file = SPIFFS.open("/wifi.json", FILE_WRITE);
    if (!file)
    {
        Serial.println("Failed to open file for writing");
        return;
    }

    // Create the JSON object
    JsonDocument doc;
    doc["ssid"] = ssid;
    doc["password"] = password;

    serializeJson(doc, Serial);
    Serial.println();

    if (serializeJson(doc, file) == 0)
    {
        Serial.println("Failed to write to file");
    }
    file.close();

    // Open the file for reading to validate the content
    File fileR = SPIFFS.open("/wifi.json", FILE_READ);
    if (!fileR)
    {
        Serial.println("Failed to open file for reading");
        return;
    }

    // Read the content back into a string for comparison
    String fileContent;
    while (fileR.available())
    {
        fileContent += (char)fileR.read();
    }
    fileR.close();

    String serializedJson;
    serializeJson(doc, serializedJson);

    if (fileContent == serializedJson)
    {
        Serial.println("File written successfully and content verified.");
    }
    else
    {
        Serial.println("File content verification failed.");
        Serial.println("Expected:");
        Serial.println(serializedJson);
        Serial.println("Found:");
        Serial.println(fileContent);
    }
}

void writeApiKey(String apiKey)
{
	File file = SPIFFS.open("/apikey.txt", FILE_WRITE);
	if (!file)
	{
		Serial.println("Failed to open file for writing");
		return;
	}
	file.print(apiKey);
	file.close();
}

String getApiKey()
{
	File file = SPIFFS.open("/apikey.txt", FILE_READ);
	if (!file)
	{
		Serial.println("Failed to open Api file for reading");
		return "";
	}
	String apiKey = file.readString();
	file.close();
	return apiKey;
}

String getSettings()
{
	File file = SPIFFS.open("/settings.json", FILE_READ);
	if (!file)
	{
		Serial.println("Failed to open Settings file for reading");
		return "";
	}
	String settings = file.readString();
	file.close();
	return settings;
}

bool storeSettings(String settings)
{
	if (!settings || settings == "")
	{
		return false;
	}
	File file = SPIFFS.open("/settings.json", FILE_WRITE);
	if (!file)
	{
		Serial.println("Failed to open file for writing");
		return false;
	}
	file.print(settings);
	file.close();
	return true;
}

bool storeResetData(int resetDate, int maxPunkte)
{
	File file = SPIFFS.open("/resetdata.json", FILE_WRITE);
	if (!file)
	{
		Serial.println("Failed to open file for writing");
		return false;
	}

	// JSON-Dokument erstellen und Daten speichern
	JsonDocument doc;
	doc["resetDate"] = resetDate;
	doc["expectedPoints"] = maxPunkte;

	// JSON in die Datei schreiben
	if (serializeJson(doc, file) == 0)
	{
		Serial.println("Failed to write JSON to file");
		file.close();
		return false;
	}

	file.close();
	return true;
}

// Funktion zum Laden von resetdate und maxPunkte
int getResetData(const String &key)
{
	File file = SPIFFS.open("/resetdata.json", FILE_READ);
	if (!file)
	{
		Serial.println("Failed to open file for reading");
		return 0;
	}

	JsonDocument doc;
	DeserializationError error = deserializeJson(doc, file);
	file.close();

	if (error)
	{
		Serial.println("Failed to parse JSON");
		return 0;
	}

	// Gibt den Wert für den angegebenen Schlüssel zurück oder 0, falls der Schlüssel nicht existiert
	return doc[key] | 0;
}

#endif