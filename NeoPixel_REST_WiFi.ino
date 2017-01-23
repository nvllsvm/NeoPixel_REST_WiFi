/*
  NeoPixel_REST_WiFi
  Andrew Rabert (nullsum.net)
  2016-08-27

  Adapted from WiFi101_AP_Neopixels by Colin Conway (Collie147 on Arduino.cc)
*/

#include <Adafruit_NeoPixel.h>
#include <ArduinoJson.h>
#include <WiFi101.h>

#include "neopixel_config.h"
#include "wifi_config.h"

const String VERSION = "0.2.0";


Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS, PIN, STRIP_TYPE + NEO_KHZ800);

bool hasWhiteSupport(int stripType) {
    if (stripType == NEO_RGB || stripType == NEO_RBG ||
        stripType == NEO_GRB || stripType == NEO_GBR ||
        stripType == NEO_BRG || stripType == NEO_BGR) {
        return false;
    }

    return true;
}


const bool supportsWhite = hasWhiteSupport(STRIP_TYPE);

WiFiServer server(80);

String currentLine;

long previousMillis = 0;
byte colorOffset = 0;

String mode;
byte brightness;
byte red;
byte blue;
byte green;
byte white;
byte interval;
bool flashCycle;
int currentPixel;
bool forwardDirection;

bool isValidBody;
String error;

int stripeLength = 12;
uint32_t colors[] = {
    pixels.Color(255, 0, 255),
    pixels.Color(0, 255, 255),
    pixels.Color(0, 0, 255),
    pixels.Color(255, 255, 255)
};

const byte numColors = sizeof(colors) / sizeof(colors[0]);

const String ERROR_VALUE_ERROR = "ValueError";
const String ERROR_NO_COLOR = "NoColor";
const String ERROR_INVALID_MODE = "InvalidMode";
const String ERROR_INVALID_BODY = "InvalidBody";


void setup() {
    Serial.begin(9600);
    pixels.begin();
    mode = "off";

    connectToAP();
    server.begin();
}


void connectToAP() {
    do {
        WiFi.begin(SSID, PASSWORD);
        Serial.print("Connecting to SSID: ");
        Serial.println(SSID);
        delay(1000);
    }
    while (WiFi.status() != WL_CONNECTED);
}


void writeStatus(WiFiClient client) {
    StaticJsonBuffer<200> jsonBuffer;
    JsonObject& root = jsonBuffer.createObject();

    root["mode"] = mode;
    if (mode == "color") {
        root["red"] = red;
        root["green"] = green;
        root["blue"] = blue;

        if (supportsWhite) {
            root["white"] = white;
        }

        root["brightness"] = brightness;
    } else if (mode == "rainbow") {
        root["interval"] = interval;
        root["brightness"] = brightness;
    } else if (mode == "flash") {
        root["red"] = red;
        root["green"] = green;
        root["blue"] = blue;

        if (supportsWhite) {
            root["white"] = white;
        }

        root["interval"] = interval;
        root["brightness"] = brightness;
    } else if (mode == "stripe") {
        root["interval"] = interval;
        root["brightness"] = brightness;
    }

    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: application/json");
    client.print("Content-Length: ");
    client.println(root.measureLength());
    client.println();

    root.printTo(client);
}

void writeError400(WiFiClient client) {
    StaticJsonBuffer<200> jsonBuffer;
    JsonObject& root = jsonBuffer.createObject();

    root["error"] = error;

    client.println("HTTP/1.1 400 Bad Request");
    client.println("Content-Type: application/json");
    client.print("Content-Length: ");
    client.println(root.measureLength());
    client.println();
    root.printTo(client);
}


void writeError404(WiFiClient client) {
    client.println("HTTP/1.1 404 Not Found");
}


void writeError405(WiFiClient client) {
    client.println("HTTP/1.1 405 Method Not Allowed");
}


void writeInfo(WiFiClient client) {
    StaticJsonBuffer<200> jsonBuffer;
    JsonObject& root = jsonBuffer.createObject();

    root["size"] = NUMPIXELS;
    root["has_white"] = supportsWhite;
    root["version"] = VERSION;

    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: application/json");
    client.print("Content-Length: ");
    client.println(root.measureLength());
    client.println();

    root.printTo(client);
}


byte validateIntIsByte(JsonObject& root, String key, byte defaultValue) {
    if (root.containsKey(key)) {
        if (root[key].is<byte>()) {
            int value = root[key];
            if (value < 0 || value > 255) {
                isValidBody = false;
                error = ERROR_VALUE_ERROR;
            } 
            return value;
        } else {
            isValidBody = false;
            error = ERROR_VALUE_ERROR;
        }
    } else {
        return defaultValue;
    }
}


bool parseBody(String body) {
    StaticJsonBuffer<200> jsonBuffer;
    JsonObject& root = jsonBuffer.parseObject(body);

    String bodyMode = root["mode"];

    if (bodyMode == "color") {
        byte bodyRed = validateIntIsByte(root, "red", 0);
        byte bodyGreen = validateIntIsByte(root, "green", 0);
        byte bodyBlue = validateIntIsByte(root, "blue", 0);
        byte bodyWhite = validateIntIsByte(root, "white", 0);
        byte bodyBrightness = validateIntIsByte(root, "brightness", 255);

        if (isValidBody) {
            if (bodyRed == 0 && bodyGreen == 0 && bodyBlue == 0) {
                if (!supportsWhite || bodyWhite == 0) {
                    isValidBody = false;
                    error = ERROR_NO_COLOR;
                }
            }
        }

        if (isValidBody) {
            mode = bodyMode;
            red = bodyRed;
            green = bodyGreen;
            blue = bodyBlue;
            white = bodyWhite;
            brightness = bodyBrightness;
        }
    } else if (bodyMode == "rainbow") {
        byte bodyInterval = validateIntIsByte(root, "interval", 50);
        byte bodyBrightness = validateIntIsByte(root, "brightness", 255);

        if (isValidBody) { 
            mode = bodyMode;
            interval = bodyInterval;
            brightness = bodyBrightness;
        }
    } else if (bodyMode == "flash") {
        byte bodyRed = validateIntIsByte(root, "red", 0);
        byte bodyGreen = validateIntIsByte(root, "green", 0);
        byte bodyBlue = validateIntIsByte(root, "blue", 0);
        byte bodyWhite = validateIntIsByte(root, "white", 0);
        byte bodyInterval = validateIntIsByte(root, "interval", 50);
        byte bodyBrightness = validateIntIsByte(root, "brightness", 255);

        if (isValidBody) {
            if (bodyRed == 0 && bodyGreen == 0 && bodyBlue == 0) {
                if (!supportsWhite || bodyWhite == 0) {
                    isValidBody = false;
                    error = ERROR_NO_COLOR;
                }
            }
        }

        if (isValidBody) {
            mode = bodyMode;
            red = bodyRed;
            green = bodyGreen;
            blue = bodyBlue;
            white = bodyWhite;
            interval = bodyInterval;
            brightness = bodyBrightness;
        }
    } else if (bodyMode == "stripe") {
        byte bodyInterval = validateIntIsByte(root, "interval", 50);
        byte bodyBrightness = validateIntIsByte(root, "brightness", 255);

        if (isValidBody) {
            mode = bodyMode;
            interval = bodyInterval;
            brightness = bodyBrightness;
            currentPixel = 0;
            forwardDirection = true;
        }
    } else if (bodyMode == "off") {
        mode = bodyMode;
    } else {
        isValidBody = false;
        error = ERROR_INVALID_MODE;
    }
}


void loop() {
    WiFiClient client = server.available();

    if (client) {
        handleRequest(client);
    }

    setNeoPixels();

    if (WiFi.status() != WL_CONNECTED) {
        connectToAP();
    }
}


void handleRequest(WiFiClient client) {
    Serial.println("New Request");

    currentLine = "";
    bool firstLine = true;
    int contentLength;
    String method;
    String path;
    String bodyContent;

    while (client.connected()) {
        if (client.available()) {
            char c = client.read();
            Serial.write(c);
            if (c == '\n') {
                if (firstLine) {
                    firstLine = false;
                    byte firstSpaceIndex = currentLine.indexOf(' '); 
                    byte secondSpaceIndex = currentLine.indexOf(' ', firstSpaceIndex+1); 
					method = currentLine.substring(0, firstSpaceIndex);
                    path = currentLine.substring(firstSpaceIndex+1, secondSpaceIndex);
                }
                if (contentLength) {
                    String workingLine = currentLine;
                    workingLine.toLowerCase();
                    if (workingLine.startsWith("content-length:")) {
                        contentLength = workingLine.substring(15, workingLine.length()).toInt();
                        isValidBody = true;
                    }
                }

                if (currentLine.length() == 0) {
                    if (path == "/") {
						if (method == "GET") {
							writeStatus(client);
						} else if (method == "PUT") {
							if (contentLength > 0) {
								for (int i = 0; i < contentLength; i++) {
									if (client.available()) {
										char c = client.read();
										if (c == '\r') {
											isValidBody = false;
                                            error = ERROR_INVALID_BODY;
											break;
										}
										bodyContent += c;
									} else {
										isValidBody = false;
                                        error = ERROR_INVALID_BODY;
										break;
									}
								}
								Serial.println(bodyContent);
								parseBody(bodyContent);

								if (isValidBody) {
									writeStatus(client);
								} else {
									writeError400(client);
								}
							} else {
								writeError400(client);
							}
						} else {
                            writeError405(client);
						}
                    } else if (path == "/info") {
						if (method == "GET") {
							writeInfo(client);	
						} else {
                            writeError405(client);
						}
                    } else {
                        writeError404(client);
                    }
                    break;
                }
                else {
                    currentLine = "";
                }
            }
            else if (c != '\r') {
                currentLine += c;
            }
        }
    }
    client.stop();
    Serial.println("End Request");
}



void setNeoPixels() {
    if (mode == "off") {
        setOff();
    } else if (mode == "color") {
        setAllColor(red, green, blue, white);
    } else if (mode == "rainbow") {
        setRainbow();
    } else if (mode == "flash") {
        setFlash();
    } else if (mode == "stripe") {
        setStripe();
    }
}


void setOff() {
    setAllColor(0,0,0,0);
}


long getRainbowPixelColor(byte offset) {
    offset = 255 - offset;
    if (offset < 85) {
        return pixels.Color(255 - offset * 3, 0, offset * 3);
    } else if (offset < 170) {
        offset -= 85;
        return pixels.Color(0, offset * 3, 255 - offset * 3);
    } else {
        offset -= 170;
        return pixels.Color(offset * 3, 255 - offset * 3, 0);
    }
}


void setAllColor(byte r, byte g, byte b, byte w) {
    for (int i = 0; i < pixels.numPixels(); i ++) {
        pixels.setPixelColor(i, pixels.Color(r, g, b, w));
    }
    pixels.setBrightness(brightness);
    pixels.show();
}


void setRainbow() {
    if (millis() - previousMillis > interval * 2) {
        for (int i = 0; i < pixels.numPixels(); i++) {
            pixels.setPixelColor(i, getRainbowPixelColor((i + colorOffset) & 255));
        }

        pixels.setBrightness(brightness);
        pixels.show();

        colorOffset++;
        previousMillis = millis();
    }
}


void setFlash() {
    if (millis() - previousMillis > interval * 2) {
        if (flashCycle) {
            setAllColor(red, green, blue, white);
            flashCycle = false;
        } else {
            setOff();
            flashCycle = true;
        }

        previousMillis = millis();
    }
}


void setStripe() {
    if (millis() - previousMillis > interval * 2) {
        if (currentPixel == 0 and !forwardDirection) {
            forwardDirection = true;
        } else if (currentPixel == pixels.numPixels()) {
            forwardDirection = false;
            currentPixel--;
        }

        setStuff(colors, numColors, currentPixel);

        if (forwardDirection) {
            currentPixel++;
        } else {
            currentPixel--;
        }

        previousMillis = millis();
    }
}


uint8_t splitColor ( uint32_t c, char value ) {
  switch ( value ) {
    case 'r': return (uint8_t)(c >> 16);
    case 'g': return (uint8_t)(c >>  8);
    case 'b': return (uint8_t)(c >>  0);
    default:  return 0;
  }
}


void setStuff(uint32_t colors[], byte numColors, int i) {
    int count = stripeLength;

    byte state = 0;
    for (int y = 0; y < pixels.numPixels(); y++) {
        int pixel;
        if (y + i >= pixels.numPixels()) {
            pixel = (y+i) - pixels.numPixels();
        } else {
            pixel = y + i;
        }
        uint32_t blend_color;
        uint32_t color;
        count--;

        if (count == 0) {
            state++;
            if (state == numColors) {
                state = 0;
            }
            count = stripeLength;
        }

        byte multiplier_a;

        if (count < (stripeLength / 2)) {
          multiplier_a = count;
          if (state == 0) {
            blend_color = colors[numColors - 1];
          } else {
            blend_color = colors[state - 1];
          }
        } else {
          multiplier_a = count - (stripeLength / 2);
          if (state == (numColors - 1)) {
            blend_color = colors[0];
          } else {
            blend_color = colors[state + 1];
          }
        }

        float multiplier = 0.5 + (multiplier_a * (0.5 / (numColors / 2)));

          byte r1 = splitColor(colors[state], 'r');
          byte g1 = splitColor(colors[state], 'g');
          byte b1 = splitColor(colors[state], 'b');
          byte r2 = splitColor(blend_color, 'r');
          byte g2 = splitColor(blend_color, 'g');
          byte b2 = splitColor(blend_color, 'b');
          Serial.println(r1);

          byte r = ((r1 * multiplier) + (r2 * (1 - multiplier))) / 2;
          byte g = ((g1 * multiplier) + (g2 * (1 - multiplier))) / 2;
          byte b = ((b1 * multiplier) + (b2 * (1 - multiplier))) / 2;

          color = pixels.Color(r,g,b,0);
        color = colors[state];
        pixels.setPixelColor(pixel, color);
    }

    pixels.setBrightness(brightness);
    pixels.show();
}
