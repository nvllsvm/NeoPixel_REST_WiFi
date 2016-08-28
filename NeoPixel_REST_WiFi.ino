/*
  NeoPixel_REST_WiFi
  Andrew Rabert (nullsum.net)
  2016-08-27

  Adapted from WiFi101_AP_Neopixels by Colin Conway (Collie147 on Arduino.cc)
*/

#include <Adafruit_NeoPixel.h>
#include <ArduinoJson.h>
#include <WiFi101.h>

const String VERSION = "0.1.0";

const byte PIN = 12;
const short NUMPIXELS = 60;

const char SSID[] = "";
const char PASSWORD[] = "";

const int STRIP_TYPE = NEO_GRBW;


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

bool isValidBody;
String error;

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
