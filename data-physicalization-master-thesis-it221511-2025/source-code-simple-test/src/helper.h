#ifndef HELPER_H
#define HELPER_H

#include <map>
#include <set>

const char *apiUrlInteraction = "https://pocketbase-data-phyz.lj-dev.at/api/collections/interactions/records";
const char *apiUrlTemp = "https://pocketbase-data-phyz.lj-dev.at/api/collections/temperature/records";
const char *apiKey = "A1JSD2OFIANJ3FL2SK";

// time sync
int64_t baseUnixTime = 0;
unsigned long baseMillis = 0;
const unsigned long syncInterval = 86400000; // 24h

// Helper function to convert ISO 8601 string to Unix timestamp (in seconds)
int64_t parseUnixTimestamp(String dateTime)
{
	struct tm tm = {0};
	int year, month, day, hour, minute, second;

	// Remove milliseconds and Z from the dateTime string if they exist
	int dotIndex = dateTime.indexOf('.');
	if (dotIndex != -1)
	{
		dateTime = dateTime.substring(0, dotIndex); // Strip milliseconds and timezone
	}
	dateTime.replace(' ', 'T'); // Replace space with T for consistent parsing

	// Parse the date and time components from the string
	sscanf(dateTime.c_str(), "%d-%d-%dT%d:%d:%d", &year, &month, &day, &hour, &minute, &second);

	// Populate the struct tm
	tm.tm_year = year - 1900; // Year since 1900
	tm.tm_mon = month - 1;	  // Month, where 0 = January
	tm.tm_mday = day;
	tm.tm_hour = hour;
	tm.tm_min = minute;
	tm.tm_sec = second;
	tm.tm_isdst = 0; // Not considering daylight saving

	// Convert to Unix timestamp and return as int64_t
	time_t timestamp = mktime(&tm);
	return timestamp == -1 ? -1 : (int64_t)timestamp; // Return -1 if mktime failed
}

int64_t currentTimestamp()
{

	if (WiFi.status() == WL_CONNECTED)
	{
		HTTPClient http;
		http.begin(apiUrlInteraction);
		http.addHeader("Content-Type", "application/json");

		// Send interaction for "timestamp"
		String requestBody = "{";
		requestBody += "\"prototypeId\":" + String(ESP.getEfuseMac()) + ",";
		requestBody += "\"interactionType\":\"timestamp\",";
		requestBody += "\"api_key\":\"" + String(apiKey) + "\"";
		requestBody += "}";

		int httpResponseCode = http.POST(requestBody);

		int64_t timestamp = 0; // Default value for failure

		if (httpResponseCode > 0)
		{
			String payload = http.getString();

			// Parse JSON response
			JsonDocument doc;
			DeserializationError error = deserializeJson(doc, payload);

			if (error)
			{
				Serial.print("JSON Parsing Error: ");
				Serial.println(error.c_str());
			}
			else
			{
				// Extract "created" field and convert to timestamp
				String createdTime = doc["created"].as<String>();
				timestamp = parseUnixTimestamp(createdTime);
			}
		}
		else
		{
			Serial.println("HTTP request failed");
		}

		http.end();
		return timestamp;
	}
	else
	{
		Serial.println("No WiFi connection.");
		return 0;
	}
}

int64_t getCurrentTime()
{
	static int64_t initialTimestamp = currentTimestamp(); // Fetch timestamp once
	return initialTimestamp + millis() / 1000;			  // Add elapsed seconds
}

int calculateExpectedPoints(long startTimestamp, int timespanInSeconds = 1209600)
{
	// API-Schlüssel abrufen
	String apiKey = getApiKey();

	// HTTP-Client konfigurieren
	HTTPClient http;
	http.begin("https://api.flatastic-app.com/index.php/api/chores/");
	http.addHeader("Content-Type", "application/json");
	http.addHeader("X-API-KEY", apiKey);

	// API-Anfrage
	int httpCode = http.GET();
	if (httpCode != HTTP_CODE_OK)
	{
		Serial.println("Fehler bei der API-Anfrage: " + String(httpCode));
		http.end();
		return 0;
	}

	// JSON-Daten parsen
	String payload = http.getString();
	JsonDocument doc;
	DeserializationError error = deserializeJson(doc, payload);
	http.end();
	if (error)
	{
		Serial.println("JSON-Parsing-Fehler: ");
		Serial.println(error.c_str());
		return 0;
	}

	int totalExpectedPoints = 0;
	for (JsonObject chore : doc.as<JsonArray>())
	{
		int points = chore["points"];
		int rotationTime = chore["rotationTime"];
		long timeLeftNext = chore["timeLeftNext"];

		// Nur Aufgaben berücksichtigen, die Punkte haben und wiederkehrend sind
		if (points > 0 && rotationTime != -1)
		{
			// Negative `timeLeftNext`-Werte auf 0 setzen (Aufgabe steht bald an)
			if (timeLeftNext < 0)
			{
				timeLeftNext = 0;
			}

			// Berechne den Zeitpunkt, an dem die Aufgabe das nächste Mal fällig ist
			long nextDueTime = startTimestamp + timeLeftNext;

			// Berechne, wie oft die Aufgabe innerhalb des Zeitraums von `startTimestamp` bis `startTimestamp + timespanInSeconds` ausgeführt wird
			int countWithinTimespan = (timespanInSeconds - timeLeftNext) / rotationTime + 1;

			// Berechne die erwarteten Punkte und addiere sie
			totalExpectedPoints += points * countWithinTimespan;
		}
	}

	totalExpectedPoints = totalExpectedPoints * 1.5;
	return totalExpectedPoints;
}

std::map<int, int> calculateUserPointsSinceReset(long resetTimestamp)
{
	// API-Schlüssel abrufen
	String apiKey = getApiKey();

	// HTTP-Client konfigurieren
	HTTPClient http;
	http.begin("https://api.flatastic-app.com/index.php/api/chores/history");
	http.addHeader("Content-Type", "application/json");
	http.addHeader("X-API-KEY", apiKey);

	// API-Anfrage
	int httpCode = http.GET();
	if (httpCode != HTTP_CODE_OK)
	{
		Serial.println("Fehler bei der API-Anfrage: " + String(httpCode));
		http.end();
		return {};
	}

	// JSON-Daten parsen
	String payload = http.getString();
	JsonDocument doc;
	DeserializationError error = deserializeJson(doc, payload);
	http.end();
	if (error)
	{
		Serial.println("JSON-Parsing-Fehler: ");
		Serial.println(error.c_str());
		return {};
	}

	// Map zum Speichern der Punkte pro Benutzer (userId)
	std::map<int, int> userPoints;

	// Iteriere über die Liste und berechne die Punkte für jeden Benutzer
	for (JsonObject chore : doc.as<JsonArray>())
	{
		int userId = chore["userId"];
		int points = chore["points"];
		long completedAt = chore["completedAt"];

		// Nur Punkte berücksichtigen, die nach dem Reset-Zeitpunkt abgeschlossen wurden
		if (completedAt > resetTimestamp)
		{
			// Punkte für den Benutzer hinzufügen oder neu initialisieren
			userPoints[userId] += points;
		}
	}
	for (const auto &entry : userPoints)
	{
		Serial.print("User ID: ");
		Serial.print(entry.first);
		Serial.print(", Points: ");
		Serial.println(entry.second);
	}

	return userPoints; // Gibt eine Map zurück, die userId und deren gesammelte Punkte enthält
}

void sendInteraction(String interactionType)
{
	if (WiFi.status() == WL_CONNECTED)
	{
		HTTPClient http;
		http.begin(apiUrlInteraction);
		http.addHeader("Content-Type", "application/json");

		// JSON-Body erstellen
		String requestBody = "{";
		requestBody += "\"prototypeId\":" + String(ESP.getEfuseMac()) + ",";
		requestBody += "\"interactionType\":\"" + interactionType + "\",";
		requestBody += "\"api_key\":\"" + String(apiKey) + "\"";
		requestBody += "}";

		// POST-Request senden
		int httpResponseCode = http.POST(requestBody);

		// Antwort vom Server auslesen
		if (httpResponseCode > 0)
		{
			String response = http.getString();
		}
		else
		{
			Serial.println("Fehler bei der HTTP-Anfrage: " + String(httpResponseCode));
		}

		http.end();
	}
	else
	{
		Serial.println("Keine WLAN-Verbindung.");
	}
}

void sendTemperature(float temperature)
{
	if (WiFi.status() == WL_CONNECTED)
	{
		HTTPClient http;
		http.begin(apiUrlTemp);
		http.addHeader("Content-Type", "application/json");

		// JSON-Body erstellen
		String requestBody = "{";
		requestBody += "\"prototypeId\":" + String(ESP.getEfuseMac()) + ",";
		requestBody += "\"temperature\":" + String(temperature) + ",";
		requestBody += "\"api_key\":\"" + String(apiKey) + "\"";
		requestBody += "}";

		// POST-Request senden
		int httpResponseCode = http.POST(requestBody);

		// Antwort vom Server auslesen
		if (httpResponseCode > 0)
		{
			String response = http.getString();
		}
		else
		{
			Serial.println("Fehler bei der HTTP-Anfrage: " + String(httpResponseCode));
		}

		http.end();
	}
	else
	{
		Serial.println("Keine WLAN-Verbindung.");
	}
}

std::set<int> checkOverdueTasksPerUser()
{
	// Set to store user IDs with overdue tasks
	std::set<int> usersWithOverdueTasks;

	// Retrieve the API key
	String apiKey = getApiKey();
	if (apiKey.isEmpty())
	{
		Serial.println("API key is missing. Please log in first.");
		return usersWithOverdueTasks; // Return empty set if no API key
	}

	// Set up HTTP client and request
	HTTPClient http;
	http.begin("https://api.flatastic-app.com/index.php/api/chores/");
	http.addHeader("Content-Type", "application/json");
	http.addHeader("X-API-KEY", apiKey);

	// Perform GET request
	int httpResponseCode = http.GET();
	if (httpResponseCode != HTTP_CODE_OK)
	{
		Serial.println("Error retrieving chores: " + String(httpResponseCode));
		http.end();
		return usersWithOverdueTasks; // Return empty set if request failed
	}

	// Parse the JSON response
	String payload = http.getString();
	JsonDocument doc;
	DeserializationError error = deserializeJson(doc, payload);
	http.end();
	if (error)
	{
		Serial.println("JSON Parsing Error: ");
		Serial.println(error.c_str());
		return usersWithOverdueTasks; // Return empty set if parsing failed
	}

	// Iterate through the list of chores
	for (JsonObject chore : doc.as<JsonArray>())
	{
		int rotationTime = chore["rotationTime"];
		long timeLeftNext = chore["timeLeftNext"];
		int userId = chore["users"][0]; // Get the first user ID in the array

		// Check if the chore meets the overdue criteria
		if (rotationTime != -1 && timeLeftNext < 0)
		{
			usersWithOverdueTasks.insert(userId);
		}
	}

	return usersWithOverdueTasks;
}

void normalizeArray(int *values, int length, int expectedPoints)
{
	if (expectedPoints == 0)
		return;

	float scaleFactor = 4095.0 / expectedPoints;

	for (int i = 0; i < length; i++)
	{
		values[i] = round(values[i] * scaleFactor);
	}
}

class CaptiveRequestHandler : public AsyncWebHandler
{
public:
	CaptiveRequestHandler() {}
	virtual ~CaptiveRequestHandler() {}
	bool canHandle(AsyncWebServerRequest *request)
	{
		request->addInterestingHeader("ANY");
		return true;
	}
	void handleRequest(AsyncWebServerRequest *request)
	{
		request->send(SPIFFS, "/index.html", "text/html", false);
		return;
	}
};

#endif // HELPER_H