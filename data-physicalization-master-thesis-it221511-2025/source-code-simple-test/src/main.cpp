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

PIRSensor pirSensor;

const char *accessPointName = "Data Physicalization";

DNSServer dnsServer;
AsyncWebServer server(80);

bool settingsUpdated = true;
bool changeLedColor = true;
bool sliderHasMoved = false;
int heightColumn1 = 0;
int heightColumn2 = 0;
bool withAnimation1 = false;
bool withAnimation2 = false;

ColumnsStruct columns({{Pot(5, 21, 20, 19), 15}, {Pot(4, 48, 45, 35), 7}});

void setupServer()
{
	server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
			  { request->send(SPIFFS, "/index.html", "text/html"); });
	server.on("/save_settings", HTTP_POST, [](AsyncWebServerRequest *request)
			  {
				  String settings = "";
				  int params = request->params();
				  for (int i = 0; i < params; i++)
				  {
					  AsyncWebParameter *p = request->getParam(i);
					  if (p->isPost())
					  {
						  if (p->name() == "settings")
						  {
							  settings = p->value().c_str();
							  
							  DynamicJsonDocument doc(1024);
							  DeserializationError error = deserializeJson(doc, settings);
							  
							  if (error) {
								  request->send(400, "application/json", "{\"success\":false, \"msg\":\"Failed to parse JSON settings.\"}");
								  return;
							  }
			  
							  bool success = storeSettings(settings);
							  if (!success) {
								  request->send(400, "application/json", "{\"success\":false, \"msg\":\"Could not open file or settings are empty.\"}");
								  return;
							  }
			  
							  JsonArray settingsArray = doc.as<JsonArray>();
							  for (JsonVariant v : settingsArray) {
								  int column = v["column"].as<int>();
								  int height = v["height"].as<int>();
								  
								  if (column == 1) {
									  heightColumn1 = height;
									  withAnimation1 = v["withAnimation"].as<bool>();
								  } else if (column == 2) {
									  heightColumn2 = height;
									  withAnimation2 = v["withAnimation"].as<bool>();
								  }
							  }
			  
							  request->send(200, "application/json", "{\"success\":true, \"msg\":\"Settings saved and positions updated successfully.\"}");
							  settingsUpdated = true;
							  return;
						  }
					  }
				  }
				  request->send(400, "application/json", "{\"success\":false, \"msg\":\"No settings data received.\"}"); });
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
}

void testFunctionOnSetup()
{
	Serial.println("initial test");
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
	initStore();

	testFunctionOnSetup();

	WiFi.mode(WIFI_AP_STA);
	WiFi.softAP(accessPointName);

	setupServer();
	dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
	dnsServer.start(53, "*", WiFi.softAPIP());
	server.addHandler(new CaptiveRequestHandler()).setFilter(ON_AP_FILTER);
	server.begin();
}

unsigned long lastPirCheck = 0;

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

	Pot pot = columns.getPot(0);
	Pot pot1 = columns.getPot(1);
	int fader_pos = analogRead(pot.VAL);
	int fader_pos1 = analogRead(pot1.VAL);

	int motionStatus = pirSensor.getMotionStatus();

	if (changeLedColor)
	{
		updateLedColors();
		changeLedColor = false;
	}

	if (settingsUpdated)
	{
		changeLedColor = true;
		settingsUpdated = false;
		move_to_position(columns, 0, heightColumn1, withAnimation1);
		move_to_position(columns, 1, heightColumn2, withAnimation1);
	}

	if (motionStatus == 2)
	{
		changeLedColor = true;
	}

	if (motionStatus == -1)
	{
		turnOffLEDStrips();
	}
}
