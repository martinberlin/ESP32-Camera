#include "OV2640.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>
#include "time.h"
#define ENABLE_OLED 

#ifdef ENABLE_OLED
#include "SSD1306.h"
#define OLED_ADDRESS 0x3c
#define I2C_SDA 14
#define I2C_SCL 13
SSD1306Wire display(OLED_ADDRESS, I2C_SDA, I2C_SCL, GEOMETRY_128_32);
#endif

#define HTTPS_HOST              "www.bigiot.net"
#define HTTPS_PORT              443
#define BIGIOT_API_KEY          ""      // Put your API Key here
#define BIGIOT_DEVICE_ID        "9110"
#define BIGIOT_INTERFACE_ID     "7832"
const char *ssid =     "KabelBox-A210";         // Put your SSID here
const char *password = "14237187131701431551";

long photoCount = 0;
const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec =      3600;
const int daylightOffset_sec =  0;

char *request_content = "--------------------------ef73a32d43e7f04d\r\n"
                        "Content-Disposition: form-data; name=\"data\"; filename=\"%s.jpg\"\r\n"
                        "Content-Type: image/jpeg\r\n\r\n";

char *request_end = "\r\n--------------------------ef73a32d43e7f04d--\r\n";


OV2640 cam;
WiFiClientSecure client;
StaticJsonBuffer<512> jsonBuffer;

void update_image(void)
{
    char status[64] = {0};
    char buf[1024];
    struct tm timeinfo;

    cam.run();

    if (!client.connect(HTTPS_HOST, HTTPS_PORT))
    {
        Serial.println("Connection failed");
        return;
    }

    if (!getLocalTime(&timeinfo))
    {
        Serial.println("Failed to obtain time");
        snprintf(buf, sizeof(buf), request_content, String(millis()).c_str());
    }
    else
    {
        strftime(status, sizeof(status), "%Y%m%d%H%M%S", &timeinfo);
        snprintf(buf, sizeof(buf), request_content, status);
    }

    uint32_t content_len = cam.getSize() + strlen(buf) + strlen(request_end);


    String request = "POST /pubapi/uploadImg/did/"BIGIOT_DEVICE_ID"/inputid/"BIGIOT_INTERFACE_ID" HTTP/1.1\r\n";
    request += "Host: www.bigiot.net\r\n";
    request += "User-Agent: TTGO-Camera-Demo\r\n";
    request += "Accept: */*\r\n";
    request += "API-KEY: " + String(BIGIOT_API_KEY) + "\r\n";
    request += "Content-Length: " + String(content_len) + "\r\n";
    request += "Content-Type: multipart/form-data; boundary=------------------------ef73a32d43e7f04d\r\n";
    request += "Expect: 100-continue\r\n";
    request += "\r\n";

    Serial.print(request);
    client.print(request);

    client.readBytesUntil('\r', status, sizeof(status));
    Serial.println(status);
    if (strcmp(status, "HTTP/1.1 100 Continue") != 0)
    {
        Serial.print("Unexpected response: ");
        client.stop();
        return;
    }

    client.print(buf);

    uint8_t *image = cam.getfb();
    size_t size = cam.getSize();
    size_t offset = 0;
    size_t ret = 0;
    while (1)
    {
        ret = client.write(image + offset, size);
        offset += ret;
        size -= ret;
        if (cam.getSize() == offset)
        {
            break;
        }
    }
    client.print(request_end);

    client.find("\r\n");

    display.setColor(BLACK);
    display.fillRect(0,22,128,20);
    display.setColor(WHITE);
        
    bzero(status, sizeof(status));
    client.readBytesUntil('\r', status, sizeof(status));
    if (strncmp(status, "HTTP/1.1 200 OK", strlen("HTTP/1.1 200 OK")))
    {
        display.drawString(0, 22, "Response status error");
        display.display();
        Serial.print("Unexpected response: ");
        Serial.println(status);
        client.stop();
        return;
    }

    if (!client.find("\r\n\r\n"))
    {
        Serial.println("Invalid response");
    }

    request = client.readStringUntil('\n');

    char *str = strdup(request.c_str());
    if (!str)
    {
        client.stop();
        return;
    }

    char *start = strchr(str, '{');

    JsonObject &root = jsonBuffer.parseObject(start);
    if (!root.success())
    {
        Serial.println("parse data fail");
        display.drawString(0, 22, "Response parse failed");
        display.display();
        client.stop();
        free(str);
        return;
    }
    if (!strcmp((const char *)root["R"], "1"))
    {
        photoCount++;
        display.drawString(0, 22, "Captures: "+String(photoCount)+" "+timeinfo.tm_hour+":"+timeinfo.tm_min);
        display.display();
    }
    free(str);
    client.stop();
}

void setup()
{
    #ifdef ENABLE_OLED
      display.init();
      display.flipScreenVertically();
      display.setFont(ArialMT_Plain_10);
    #endif
    Serial.begin(115200);
    WiFiManager wifiManager;
    //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
    wifiManager.setAPCallback(configModeCallback);
    wifiManager.setBreakAfterConfig(true); // Without this saveConfigCallback does not get fired
    wifiManager.setSaveConfigCallback(saveConfigCallback);
    wifiManager.autoConnect("AutoConnectAP");
    display.clear();
    display.drawString(0, 0,  "Camera online");
    display.drawString(0, 10, "BIGIOT device: "BIGIOT_DEVICE_ID);
    display.display();
  
    camera_config_t camera_config;
    camera_config.ledc_channel = LEDC_CHANNEL_0;
    camera_config.ledc_timer = LEDC_TIMER_0;
    camera_config.pin_d0 = 17;
    camera_config.pin_d1 = 35;
    camera_config.pin_d2 = 34;
    camera_config.pin_d3 = 5;
    camera_config.pin_d4 = 39;
    camera_config.pin_d5 = 18;
    camera_config.pin_d6 = 36;
    camera_config.pin_d7 = 19;
    camera_config.pin_xclk = 27;
    camera_config.pin_pclk = 21;
    camera_config.pin_vsync = 22;
    camera_config.pin_href = 26;
    camera_config.pin_sscb_sda = 25;
    camera_config.pin_sscb_scl = 23;
    camera_config.pin_reset = 15;
    camera_config.xclk_freq_hz = 20000000;
    camera_config.pixel_format = CAMERA_PF_JPEG;
    camera_config.frame_size = CAMERA_FS_SVGA;

    cam.init(camera_config);
  
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
}

void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
  display.drawString(0, 0, "Connect to:"+myWiFiManager->getConfigPortalSSID());
  display.drawString(0, 10, "And set up cam WiFi in:");
  display.drawString(0, 20, IpAddress2String(WiFi.softAPIP()));
  display.display();
}

void saveConfigCallback() {
  display.clear();
  display.drawString(0, 0, "Saving configuration");
  display.display();
  delay(500);
}

/**
 * Convert the IP to string so we can send the display
 */
String IpAddress2String(const IPAddress& ipAddress)
{
  return String(ipAddress[0]) + String(".") +\
  String(ipAddress[1]) + String(".") +\
  String(ipAddress[2]) + String(".") +\
  String(ipAddress[3])  ;
}

void loop()
{
    update_image();
    delay(10000);
}
