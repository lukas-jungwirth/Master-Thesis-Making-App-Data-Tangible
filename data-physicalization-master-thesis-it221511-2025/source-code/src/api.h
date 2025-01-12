#ifndef API_H
#define API_H

#include <HTTPClient.h>
#include "store.h"

String getApiPoints()
{
    String apiKey = getApiKey();
    if (apiKey == "")
    {
        return "";
    }
    HTTPClient http;

    http.begin("https://api.flatastic-app.com/index.php/api/chores/statistics");
    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-API-KEY", apiKey);

    int httpResponseCode = http.GET();

    if (httpResponseCode == 200)
    {
        String json = http.getString();
        http.end();
        return json;
    }
    else
    {
        return "";
    }
}

void login(String email, String password, AsyncWebServerRequest *request)
{
    if (WiFi.status() == WL_CONNECTED)
    {
        HTTPClient http;

        http.begin("https://api.flatastic-app.com/index.php/api/auth/login");
        http.addHeader("Content-Type", "application/json");

        // JSON-Daten erstellen
        JsonDocument doc;
        doc["email"] = email;
        doc["password"] = password;

        String requestBody;
        serializeJson(doc, requestBody);

        int httpResponseCode = http.POST(requestBody);

        if (httpResponseCode == 200)
        {
            // return 200
            String json = http.getString();

            JsonDocument doc;
            deserializeJson(doc, json);

            const char *key = doc["X-API-KEY"];
            if (key != NULL)
            {
				Serial.println("API KEY:");
				Serial.println(key);
                writeApiKey(key);
            }

            request->send(200, "application/json", "{\"success\":true, \"msg\":\"Successfully logged in!\"}");
            http.end();
            return;
        }
        if (httpResponseCode == 400)
        {
            request->send(400, "application/json", "{\"success\":false, \"msg\":\"Wrong credentials\"}");
            http.end();
            return;
        }
        else
        {
            request->send(400, "application/json", "{\"success\":false, \"msg\":\"Something went wrong! Please try again later.\"}");
            http.end();
            return;
        }
    }
    else
    {
        request->send(400, "application/json", "{\"success\":false, \"msg\":\"Please check your internet connection.\"}");
        return;
    }
}


void getWGData(AsyncWebServerRequest *request)
{
    if (WiFi.status() == WL_CONNECTED)
    {

        String apiKey = getApiKey();
        if (apiKey == "")
        {
            request->send(400, "application/json", "{\"success\":false, \"msg\":\"Please login first.\"}");
            return;
        }

        HTTPClient http;

        http.begin("https://api.flatastic-app.com/index.php/api/wg");
        http.addHeader("Content-Type", "application/json");
        http.addHeader("X-API-KEY", apiKey);

        int httpResponseCode = http.GET();

        if (httpResponseCode == 200)
        {
            // return 200
            String json = http.getString();

            JsonDocument doc;
            deserializeJson(doc, json);

            const char *key = doc["X-API-KEY"];

            request->send(200, "application/json", json);
            http.end();
            return;
        }
        if (httpResponseCode == 400)
        {
            request->send(400, "application/json", "{\"success\":false, \"msg\":\"Wrong credentials\"}");
            http.end();
            return;
        }
        else
        {
            request->send(400, "application/json", "{\"success\":false, \"msg\":\"Something went wrong! Please try again later.\"}");
            http.end();
            return;
        }
    }
}



#endif // API_H
