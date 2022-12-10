#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>
#include <TZ.h>
#define MYTZ TZ_Europe_Brussels

#include <GxEPD2_3C.h>
#include "GxEPD2_display_selection_new_style.h"

#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSerif9pt7b.h>

#include "WeatherIcon.h"

#include "StringSplitter.h"

#include "secrets.h"

const int FRAME_WIDTH = 640;
const int FRAME_HEIGHT = 384;

const int TOP_MARGIN = 35;
const int LEFT_MARGIN = 55;

#define NUMITEMS(arg) ((unsigned int)(sizeof(arg) / sizeof(arg[0])))

String getTasks()
{

  WiFiClient client;
  HTTPClient http;

  http.begin(client, F("http://mplanner.maartje.dev/tiny/tasks?limit=10")); // sad... but we don't have enough memory to use the secure client
  http.addHeader("Authorization", apiToken);

  // Send HTTP GET request
  int httpResponseCode = http.GET();
  if (httpResponseCode != 200)
  {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }


  DynamicJsonDocument doc(1024);
  deserializeJson(doc, http.getStream());
  JsonArray obj = doc.as<JsonArray>();

  String tasks = "";
  for (int i = 0; i < obj.size(); i++)
  {
    tasks += "* " + obj[i]["time"].as<String>() + "-" + obj[i]["name"].as<String>() + "\n";
  }

  http.end(); 

  return tasks;
}

String getCalendar()
{

  WiFiClient client;
  HTTPClient http;

  http.begin(client, F("http://mplanner.maartje.dev/tiny/calendar?limit=8")); // sad... but we don't have enough memory to use the secure client
  http.addHeader("Authorization", apiToken);

  // Send HTTP GET request
  int httpResponseCode = http.GET();
  if (httpResponseCode != 200)
  {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }


  DynamicJsonDocument doc(1024);
  deserializeJson(doc, http.getStream());
  JsonArray obj = doc.as<JsonArray>();

  String tasks = "";
  for (int i = 0; i < obj.size(); i++)
  {
    tasks += "* " + obj[i]["start"].as<String>() + " - " + obj[i]["name"].as<String>();
    if (obj[i]["location"].as<String>() != "")
    {
      tasks += "-" + obj[i]["location"].as<String>();
    }
    tasks += "\n";
  }

  http.end(); 

  return tasks;
}

String getIdeas()
{

  WiFiClient client;
  HTTPClient http;

  http.begin(client, F("http://mplanner.maartje.dev/tiny/ideas/1000738706333892618?limit=10")); // sad... but we don't have enough memory to use the secure client
  http.addHeader("Authorization", apiToken);

  // Send HTTP GET request
  int httpResponseCode = http.GET();
  if (httpResponseCode != 200)
  {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }


  DynamicJsonDocument doc(1024);
  deserializeJson(doc, http.getStream());
  JsonArray obj = doc.as<JsonArray>();

  String tasks = "";
  for (int i = 0; i < obj.size(); i++)
  {
    tasks += "* " + obj[i]["name"].as<String>() + "\n";
  
  }

  http.end(); 

  return tasks;
}

void getWeather(int8 *tmpNow, int8 *minToday, int8 *maxToday, byte *rainToday, int8 *minTomorrow, int8 *maxTomorrow,byte *rainTomorrow, char *iconToday, char *iconTomorrow)
{

  WiFiClient client;
  HTTPClient http;
  String url = String(F("http://api.openweathermap.org/data/3.0/onecall?"));
  url.concat(weatherCity);
  url.concat(F("&appid="));
  url.concat(weatherApiKey);
  url.concat(F("&exclude=hourly,minutely,alerts&units=metric"));
  http.begin(client,url);

  // Send HTTP GET request
  int httpResponseCode = http.GET();
  if (httpResponseCode != 200)
  {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
    return;
  }

  // The filter: it contains "true" for each value we want to keep
  StaticJsonDocument<200> filter;
  filter["current"]["temp"] = true;
  filter["daily"][0]["temp"]["min"] = true;
  filter["daily"][0]["temp"]["max"] = true;
  filter["daily"][0]["weather"][0]["icon"] = true;
  filter["daily"][0]["pop"] = true;

  DynamicJsonDocument doc(1024);
  deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
  JsonObject obj = doc.as<JsonObject>();

  *tmpNow = (int8)round(obj["current"]["temp"].as<float>());
  *minToday = (int8)round(obj["daily"][0]["temp"]["min"].as<float>());
  *maxToday = (int8)round(obj["daily"][0]["temp"]["max"].as<float>());
  *rainToday = (byte)(obj["daily"][0]["pop"].as<float>()*100);
  *iconToday = convertOpenWeatherToMeteocons(obj["daily"][0]["weather"][0]["icon"].as<String>());
  
  *minTomorrow = (int8)round(obj["daily"][1]["temp"]["min"].as<float>());
  *maxTomorrow = (int8)round(obj["daily"][1]["temp"]["max"].as<float>());
  *rainTomorrow = (byte)(obj["daily"][1]["pop"].as<float>()*100);
  *iconTomorrow = convertOpenWeatherToMeteocons(obj["daily"][1]["weather"][0]["icon"].as<String>());
  
  http.end(); 
}

String getLocalTime()
{
  time_t rawtime;
  struct tm *timeinfo;
  time(&rawtime);
  timeinfo = localtime(&rawtime);

  return String(asctime(timeinfo));
}

int getHours()
{
  time_t rawtime;
  struct tm *timeinfo;
  time(&rawtime);
  timeinfo = localtime(&rawtime);
  return timeinfo->tm_hour;
}

void renderDisplay()
{

  int8 tmpNow, minToday, maxToday, minTomorrow, maxTomorrow;
  byte rainToday, rainTomorrow;
  char iconToday, iconTomorrow;

  StringSplitter *taskSplitter = new StringSplitter(getTasks(), '\n', 10); 
  int taskCount = taskSplitter->getItemCount();

  StringSplitter *calendarSplitter = new StringSplitter(getCalendar(), '\n', 8);
  int calendarCount = calendarSplitter->getItemCount();

  StringSplitter *ideaSplitter = new StringSplitter(getIdeas(), '\n', 10);
  int ideaCount = ideaSplitter->getItemCount();

  getWeather(&tmpNow,  &minToday,  &maxToday,  &rainToday,  &minTomorrow,  &maxTomorrow, &rainTomorrow, &iconToday, &iconTomorrow);

  display.firstPage();
  do
  {
    display.fillScreen(GxEPD_WHITE);
    display.setFont();

    display.setTextColor(GxEPD_BLACK);

    // print day greeting
    String text = "";
    // set greeting for time of day
    int time = getHours();
    if (time < 12)
    {
      text = F("Good morning");
    }
    else if (time < 18)
    {
      text = F("Good afternoon");
    }
    else
    {
      text = F("Good evening");
    }
    display.setCursor(LEFT_MARGIN + 5, TOP_MARGIN);
    display.setFont(&FreeSerif9pt7b);
    display.print(text + F(", Maartje"));

    display.fillRect(LEFT_MARGIN + 13, TOP_MARGIN + 12, 250, 150, GxEPD_RED);
    display.fillRect(LEFT_MARGIN + 13 + 3, TOP_MARGIN + 12 + 3, 250 - 3 - 3, 150 - 6, GxEPD_WHITE);

    display.setTextColor(GxEPD_BLACK);
    display.setCursor(LEFT_MARGIN + 23, TOP_MARGIN + 29);
    display.setFont(&FreeSansBold9pt7b);
    display.println(F("Upcoming tasks: "));
    display.setFont(&FreeSans9pt7b);

    for(int i = 0; i < taskCount; i++){
      display.setCursor(LEFT_MARGIN + 17, TOP_MARGIN + 46 + i * 14);
      display.println(taskSplitter->getItemAtIndex(i).substring(0, 30));
    }

    if (taskCount <2 && taskSplitter->getItemAtIndex(0) == "" )
    {
      display.setCursor(LEFT_MARGIN + 17, TOP_MARGIN + 46);
      display.println(F("No tasks left, go hug a Blahaj"));
    }

    display.fillRect(LEFT_MARGIN + 13, TOP_MARGIN + 190, 250, 130, GxEPD_RED);
    display.fillRect(LEFT_MARGIN + 13 + 3, TOP_MARGIN + 190 + 3, 250 - 3 - 3, 130 - 6, GxEPD_WHITE);

    display.setTextColor(GxEPD_BLACK);
    display.setCursor(LEFT_MARGIN + 17, TOP_MARGIN + 205);
    display.setFont(&FreeSansBold9pt7b);
    display.println("Calendar: ");
    display.setFont(&FreeSans9pt7b);

    for(int i = 0; i < calendarCount; i++){
      display.setCursor(LEFT_MARGIN + 17, TOP_MARGIN + 220 + i * 14);
      display.println(calendarSplitter->getItemAtIndex(i).substring(0, 30));
    }

    Serial.println(calendarCount);

    if (calendarCount < 2 && calendarSplitter->getItemAtIndex(0) == "")
    {
      display.setCursor(LEFT_MARGIN + 17, TOP_MARGIN + 240);
      display.println(F("Nothing to do, enjoy!"));
    }

    // print ideas
    display.fillRect(LEFT_MARGIN + 280, TOP_MARGIN + 140, 250, 180, GxEPD_RED);
    display.fillRect(LEFT_MARGIN + 280 + 3, TOP_MARGIN + 140 + 3, 250 - 3 - 3, 180 - 6, GxEPD_WHITE);

    display.setTextColor(GxEPD_BLACK);
    display.setFont(&FreeSansBold9pt7b);
    display.setCursor(LEFT_MARGIN + 290, TOP_MARGIN + 160);
    display.println("Here are some ideas: ");
    display.setFont(&FreeSans9pt7b);

    for(int i = 0; i < ideaCount; i++){
      display.setCursor(LEFT_MARGIN + 290, TOP_MARGIN + 175 + i * 14);
      display.print(ideaSplitter->getItemAtIndex(i).substring(0, 29));
    }

    if (ideaCount == 0)
    {
      display.setCursor(LEFT_MARGIN + 17, TOP_MARGIN + 240);
      display.println(F("Go outside think of some ideas"));
    }

    // print weather
    display.setFont(&Weathericon); // Change font
    display.setTextSize(3);
    display.setCursor(470, TOP_MARGIN + 60);
    display.write(iconToday);              // Print weather icon
    display.setFont(&FreeSans9pt7b); // Change font
    display.setTextSize(2);
    display.setCursor(540, TOP_MARGIN + 40);
    display.print((int)tmpNow); // Print temperature
    display.setTextSize(1);
    display.setCursor(345, TOP_MARGIN + 69);
    display.write("Min: ");
    display.print(minToday); // Print minimum temperature
    display.write(" Max: ");
    display.print(maxToday); // Print maximum temperature
    display.write(" Rain: ");
    display.print((int)rainToday); // Print rain percentage
    display.write("%");

    display.setCursor(345, TOP_MARGIN + 90);
    display.print("Tomorrow ");    // Print weather description
    display.setFont(&Weathericon); // Change font
    display.write(iconTomorrow);              // Print weather icon
    display.setFont(&FreeSans9pt7b);      // Change font
    display.write(" ");
    display.print(minTomorrow); // Print minimum temperature
    display.write(" - ");
    display.print(maxTomorrow); // Print maximum temperature
    display.write(" Rain: ");
    display.print((int)rainTomorrow); // Print rain percentage
    display.write("%");

    // add footer
    display.setTextColor(GxEPD_RED);
    display.setCursor(LEFT_MARGIN, FRAME_HEIGHT - 7);
    display.setFont();
    display.setTextSize(1);
    display.print(F("Powered by M-Planner - last updated ") + getLocalTime());
  } while (display.nextPage());
}

void setup()
{
  Serial.begin(115200);

  // connect to wifi
  WiFi.setHostname("M-Planner e-ink");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.print("\r\nIP address: ");
  Serial.println(WiFi.localIP());

  configTime(MYTZ, "pool.ntp.org");

  time_t rawtime;
  while (getLocalTime().indexOf("1970") > -1)
  {
    Serial.println("Getting time...");
    localtime(&rawtime);
    delay(300);
  }

  Serial.println(getLocalTime());

  display.init();
  display.setRotation(2);

  renderDisplay();

  // deep sleep for 1 hour
  ESP.deepSleep((uint64_t)3600000000);
}

void loop(){};
