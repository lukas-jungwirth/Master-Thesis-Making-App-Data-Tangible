#include <WiFi.h>
#include <AsyncTCP.h>
#include "ESPAsyncWebServer.h"
#include <DNSServer.h>
#include <HTTPClient.h>
#include "SPIFFS.h"
#include <ArduinoJson.h>
#include "struct.h"
#include "pir.h"
#include "store.h"
#include "led_strip.h"
#include "slidepot.h"
#include "api.h"
#include "helper.h"
#include <esp_wifi.h>
#include "temp_sensor.h"

TempSensor tempSensor;
PIRSensor pirSensor;

const char *accessPointName = "Data Physicalization 3";

DNSServer dnsServer;
AsyncWebServer server(80);

bool settingsUpdated = true;
bool tryingToConnect = false;
int expectedPoints = 0;
int64_t currentTime = 0;
int64_t resetTime = 0;
int64_t startTimeStamp = 0;

bool changeLedColor = true;
bool sliderHasMoved = false;

unsigned long lastGetPointsTime = 0;
bool tryToReconnect = false;
unsigned long lastReconnectAttempt = 0;
unsigned long reconnectInterval = 30000;
unsigned long lastOverdueCheck = 0;
const unsigned long overdueCheckInterval = 7200000;

ColumnsStruct columns({{Pot(5, 21, 20, 19), 15}, {Pot(4, 48, 45, 35), 7}});
std::set<int> columnsWithOverdueTasks = {};

void getPoints()
{

	lastGetPointsTime = millis();
	currentTime = startTimeStamp + millis() / 1000;

	// Check if reset is necessary
	if (resetTime == 0 || currentTime - resetTime > 1209600 || expectedPoints == 0)
	{
		resetTime = currentTime;
		int exPoints = calculateExpectedPoints(resetTime);
		expectedPoints = exPoints;
		storeResetData(resetTime, expectedPoints);
	}

	// Get points for each user since reset
	std::map<int, int> chorePoints = calculateUserPointsSinceReset(resetTime);
	// Get settings
	String settings = getSettings();

	if (settings.isEmpty())
	{
		return;
	}

	JsonDocument settingsDoc;
	DeserializationError error = deserializeJson(settingsDoc, settings);
	if (error)
	{
		return;
	}

	int columnValues[columns.size()] = {0};

	// check for overdue tasks
	std::set<int> usersWithOverdueTasks = checkOverdueTasksPerUser();
	columnsWithOverdueTasks = {};

	for (JsonObject setting : settingsDoc.as<JsonArray>())
	{
		int column = setting["column"];
		int userId = setting["user"];

		if (usersWithOverdueTasks.count(userId) > 0)
		{
			columnsWithOverdueTasks.insert(column - 1);
		}

		if (column > 0 && column <= columns.size())
		{
			// Check if chorePoints[userId] exists and is valid
			if (chorePoints.find(userId) != chorePoints.end())
			{
				columnValues[column - 1] = chorePoints[userId]; // 0-based index
			}
		}
	}

	int maxPoints = expectedPoints / 2;
	normalizeArray(columnValues, columns.size(), maxPoints);

	// Set position for each column
	for (int i = 0; i < columns.size(); i++)
	{
		move_to_position(columns, i, columnValues[i]);
	}
}

void connect_wifi(String ssid, String password, bool store = false)
{
	if (store)
	{
		storeWifiCredentials(ssid, password);
	}
	WiFi.begin(ssid.c_str(), password.c_str());
	esp_wifi_set_max_tx_power(40);
}

void setupServer()
{
	server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
			  { 
                //if wifi is connected, redirect to the main page
                if (WiFi.status() == WL_CONNECTED)
                {
                    if(getApiKey() == ""){
                    request->send(SPIFFS, "/index.html", "text/html");
                    return;
                    }else{
                        request->send(SPIFFS, "/main.html", "text/html");
                        return;
                    }
                }else{
                    request->send(SPIFFS, "/managewifi.html", "text/html");
                    return;
                } });
	server.on("/main", HTTP_GET, [](AsyncWebServerRequest *request)
			  {
                if(WiFi.status() != WL_CONNECTED){
                    request->send(SPIFFS, "/managewifi.html", "text/html");
                    return;
                }
                if(getApiKey() == ""){
                    request->send(SPIFFS, "/index.html", "text/html");
                    
                    return;
                }else{
                    request->send(SPIFFS, "/main.html", "text/html");
                    return;
                } });
	server.on("/managewifi", HTTP_GET, [](AsyncWebServerRequest *request)
			  { 
                    request->send(SPIFFS, "/managewifi.html", "text/html");
					return; });
	server.on("/connecting", HTTP_GET, [](AsyncWebServerRequest *request)
			  { 
                request->send(SPIFFS, "/connecting.html", "text/html");
				return; });
	server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request)
			  { request->send(200, "application/json", "{\"status\":" + String(WiFi.status()) + "}"); });
	server.on("/login", HTTP_POST, [](AsyncWebServerRequest *request)
			  {
                String email = "";
                String password = "";
                int params = request->params();
                for (int i = 0; i < params; i++)
                {
                    AsyncWebParameter *p = request->getParam(i);
                    if (p->isPost())
                    {
                        // HTTP POST email value
                        if (p->name() == "email")
                        {
                            email = p->value().c_str();
                        }
                        // HTTP POST password value
                        if (p->name() == "password")
                        {
                            password = p->value().c_str();
                        }
                    }
                }
                if (email != "" && password != "")
                {
                    login(email.c_str(), password.c_str(), request);
                    lastGetPointsTime = 0;
					currentTime = startTimeStamp + millis() / 1000;
					resetTime = currentTime;
					int exPoints = calculateExpectedPoints(resetTime);
					expectedPoints = exPoints;
					storeResetData(resetTime, expectedPoints);
                }
                else
                {
                    request->send(400, "application/json", "{\"false\":true, \"msg\":\"Something went wrong. Please check your input. EM+PW\"}");
                } });
	server.on("/get_wg_data", HTTP_GET, [](AsyncWebServerRequest *request)
			  { getWGData(request); });
	server.on("/get_settings", HTTP_GET, [](AsyncWebServerRequest *request)
			  {
                    sendInteraction("settings");
                    String settings = getSettings();
                    if (settings == "")
                    {
                        request->send(400, "application/json", "{\"success\":false, \"msg\":\"Could not open file or settings are empty.\"}");
                        return;
                    }
                    request->send(200, "application/json", settings); });
	server.on("/save_settings", HTTP_POST, [](AsyncWebServerRequest *request)
			  {
                  // take json data and save it to the file
                  String settings = "";
                  int params = request->params();

                  for (int i = 0; i < params; i++)
                  {
                      AsyncWebParameter *p = request->getParam(i);
                      if (p->isPost())
                      {
                          // HTTP POST ssid value
                          if (p->name() == "settings")
                          {
                            settings = p->value().c_str();

                            bool success = storeSettings(settings);
                            if (!success)
                            {
                                request->send(400, "application/json", "{\"success\":false, \"msg\":\"Could not open file or settings are empty.\"}");
                                return;
                            }else {
                                request->send(200, "application/json", "{\"success\":true, \"msg\":\"Settings saved successfully.\"}");
                                settingsUpdated = true;
                                return;
                            }                            
                          }
                      }
                  } });
	server.on("/has_api_key", HTTP_GET, [](AsyncWebServerRequest *request)
			  {
                  String apiKey = getApiKey();
                  if (apiKey == "")
                  {
                      request->send(200, "application/json", "false");
                  }
                  else
                  {
                      request->send(200, "application/json", "true");
                  } });
	server.on("/connect_wifi", HTTP_POST, [](AsyncWebServerRequest *request)
			  {
                String ssid = "";
                String pw = "";
                int params = request->params();
      for(int i=0;i<params;i++){
        AsyncWebParameter* p = request->getParam(i);
        if(p->isPost()){
          // HTTP POST ssid value
          if (p->name() == "ssid") {
            ssid = p->value().c_str();
          }
          // HTTP POST pass value
          if (p->name() == "password") {
            pw = p->value().c_str();
          }
        }
        }
        if(ssid != "" && pw != ""){
            connect_wifi(ssid, pw, true);
            request->send(200, "application/json", "{\"success\":true, \"msg\":\"Started connecting to wifi\"}");
            return;
        }
        else{
            request->send(400, "application/json", "{\"success\":false, \"msg\":\"Missing SSID or password\"}");
            return;
        } 
        request->send(500, "application/json", "{\"success\":false, \"msg\":\"Sorry, something went wrong.\"}");
            return; });
	server.on("/reset", HTTP_GET, [](AsyncWebServerRequest *request){
		resetTime = 0;
		expectedPoints = 0;
		request->send(200, "application/json", "{\"success\":true, \"msg\":\"Reset successful.\"}");
		return;
	});
}

void testFunctionOnSetup()
{
	for (size_t i = 0; i < 7; i++)
	{
		if (i % 2 == 0)
		{
			setSolidColor(0, CRGB::Orange);
			setSolidColor(1, CRGB::Orange);
		}
		else
		{
			setSolidColor(0, CRGB::Black);
			setSolidColor(1, CRGB::Black);
		}
		delay(500);
	}
}

void setup()
{

	Serial.begin(115200);

	Serial.println(ESP.getEfuseMac());

	initStore();
	pirSensor.initPir();
	initSlidePot(columns);
	initLEDStrip(columns);
	tempSensor.initTempSensor();

	testFunctionOnSetup();

	String storedSSID = getWifiSSID();
	String storedPassword = getWifiPassword();

	resetTime = getResetData("resetDate");
	expectedPoints = getResetData("expectedPoints");

	WiFi.mode(WIFI_AP_STA);
	WiFi.softAP(accessPointName);

	if (!storedSSID.isEmpty() && !storedPassword.isEmpty())
	{
		WiFi.begin(storedSSID.c_str(), storedPassword.c_str());
		connect_wifi(storedSSID, storedPassword);

		// Attempt connection for 10 seconds
		unsigned long startAttemptTime = millis();
		while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000)
		{
			delay(500);
			Serial.print(".");
		}

		if (WiFi.status() == WL_CONNECTED)
		{
			esp_wifi_set_max_tx_power(40);
			sendInteraction("reconnect");
		}
		else
		{
			WiFi.disconnect(true); // Disconnect in case of failed attempt
		}
	}

	setupServer();
	dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
	dnsServer.start(53, "*", WiFi.softAPIP());
	server.addHandler(new CaptiveRequestHandler()).setFilter(ON_AP_FILTER);
	server.begin();
}

unsigned long lastPirCheck = 0;

bool checkAndReconnectWiFi()
{

	if (WiFi.status() == WL_CONNECTED)
	{
		if(tryToReconnect){
			turnOffLEDStrips();
			tryToReconnect = false;
		}
		return true;
	}

	if (millis() - lastReconnectAttempt >= reconnectInterval)
	{
		Serial.println("Starting Wi-Fi reconnect attempt...");
		WiFi.disconnect();
		WiFi.begin(getWifiSSID().c_str(), getWifiPassword().c_str());
		lastReconnectAttempt = millis();
	}

	tryToReconnect = true;
	return false;
}

void updateLedColors(bool isError = false)
{
	for (int i = 0; i < columns.size(); i++)
	{
		CRGB color = getColorFromStore(i);
		if (isError)
		{
			color = CRGB::Red;
		}
		setSolidColor(i, color);
	}
}

void loop()
{
	dnsServer.processNextRequest();

	if (checkAndReconnectWiFi())
	{

		int motionStatus = pirSensor.getMotionStatus();
		tempSensor.checkTemperature();

		if (changeLedColor)
		{
			updateLedColors();
			changeLedColor = false;
		}

		if (settingsUpdated || startTimeStamp == 0 ||
			(motionStatus == 2 || (motionStatus == 1 && millis() - lastGetPointsTime > 30000)))
		{
			if (startTimeStamp == 0){
				startTimeStamp = currentTimestamp();
			}
			getPoints();
			changeLedColor = true;
			settingsUpdated = false;
		}

		if (motionStatus == 2)
		{
			sendInteraction("first");
			// check for all columns if they have a task that is overdue
			if (!columnsWithOverdueTasks.empty())
			{
				for (int i = 0; i < columns.size(); i++)
				{
					if (columnsWithOverdueTasks.count(i) > 0 && (millis() - lastOverdueCheck > overdueCheckInterval || lastOverdueCheck == 0))
					{
						setSolidColor(i, CRGB::Red);
						moveColumnUpandDown(columns, i);
						changeLedColor = true;
						lastOverdueCheck = millis();
					}
				}
			}
			changeLedColor = true;
		}

		if (motionStatus == -1)
		{
			sendInteraction("stop");
			turnOffLEDStrips();
		}
	}
	else
	{
		updateLedColors(true);
	}
}
