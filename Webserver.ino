// DNSServer had to be patched, info found here: https://github.com/espressif/arduino-esp32/issues/1037
// https://github.com/me-no-dev/ESPAsyncWebServer

const char* ssid = "Linksys05107";
const char* password = "xt6lkq0724";
String ip = "";

AsyncWebServer server(80);

void Webserver_Init()
{
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    Serial.println("Connecting to wifi");

    int waitCount = 0;
    while (WiFi.status() != WL_CONNECTED) 
    {  
      delay(500);
      Serial.print(".");
      waitCount ++;

      pixels.setPixelColor(0, pixels.Color(200,150,255));
      pixels.show();

      if (waitCount > 10) {
        ESP.restart();
      }
    }

    ip = String(WiFi.localIP());
    Serial.println("");
    Serial.print("Connected to ");
    Serial.println(ssid);
    Serial.print("IP address: ");
    Serial.println(ip);
    Serial.println("Starting MDNS as 'hamhamhub'");
  
    if (MDNS.begin("hamhamhub")) 
    {
      Serial.println("MDNS responder started");
    }

    // Add service to MDNS-SD
    MDNS.addService("http", "tcp", 80);
    
    pixels.setPixelColor(0, pixels.Color(0,255,0));
    pixels.show();
    getTemperatures(true);

    // setup all of the api endpoints for the control webpage to use
    server.on(
      "/prepstate", 
      HTTP_GET, 
      [](AsyncWebServerRequest * request) {

      Serial.println("Prepping state");
      getTemperatures(false);
      
      // note: we're 'cache busting' all of these responses so the browser re-requests the api every time
      AsyncWebServerResponse *response = request->beginResponse(200, "text/html", "OK");
      request->beginResponse(200);
      response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
      response->addHeader("Pragma", "no-cache");
      response->addHeader("Expires", "-1");
      request->send(response);
    });

    server.on(
      "/state", 
      HTTP_GET, 
      [](AsyncWebServerRequest * request) {

      Serial.println("Sending state");
      
      String result;
      result += String(lastTemp);
      result += F("|");
      result += String(lastHum);

      Serial.println(result);
      
      // note: we're 'cache busting' all of these responses so the browser re-requests the api every time
      AsyncWebServerResponse *response = request->beginResponse(200, "text/html", result);
      request->beginResponse(200);
      response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
      response->addHeader("Pragma", "no-cache");
      response->addHeader("Expires", "-1");
      request->send(response);
    });

    server.on(
      "/history", 
      HTTP_GET, 
      [](AsyncWebServerRequest * request) {

      Serial.println("Prepping state");
      getTemperatures(false);
      
      Serial.print("Sending history of ");
      Serial.print(readingIndex);
      Serial.println(" states.");

      String result;
      
      for (byte i = 0; i < readingIndex; i ++) 
      {
        result += String(temps[i]);
        result += F("|");
        result += String(hums[i]);

        if (i < (readingIndex - 1)) {
          result += F("|");
        }
      }

      Serial.println(result);
      
      // note: we're 'cache busting' all of these responses so the browser re-requests the api every time
      AsyncWebServerResponse *response = request->beginResponse(200, "text/html", result);
      request->beginResponse(200);
      response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
      response->addHeader("Pragma", "no-cache");
      response->addHeader("Expires", "-1");
      request->send(response);
    });
  
    // if the url doesn't get picked up as an api endpoint, assume it's a file from SPIFFS
    server.onNotFound(
        [](AsyncWebServerRequest *request) {
          
            Serial.println("Serving not found");
            // if the url is not recognized as an api endpoint and doesn't match a local file, 
            // redirect it to /index.html
            if (!handleFileRead(request)) {
              
                redirectToIndex(request);
            }
        });

    server.begin();
}

void Webserver_Update(void)
{
}

// send the right file to the client (if it exists)
bool handleFileRead(AsyncWebServerRequest *request)
{
    String path = request->url();
    
    // If a folder is requested, send the index file
    if (path.endsWith("/")) {
      
      path += "index.html";
    }

    // Get the MIME type
    String contentType = getContentType(path);

    // does this file exist in SPIFFS?
    if (SPIFFS.exists(path))
    {
        Serial.println("Serving SPIFFS file");
        // send it
        request->send(SPIFFS, path, contentType);
        return true;
    }

    // If the file doesn't exist, return false
    return false;
}

void redirectToIndex(AsyncWebServerRequest *request)
{  
    Serial.println("Serving redirect");      
    // note: we're 'cache busting' all of these responses so the browser re-requests the api every time
    AsyncWebServerResponse *response = request->beginResponse(302, "text/plain", "");
    response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    response->addHeader("Pragma", "no-cache");
    response->addHeader("Expires", "-1");
    response->addHeader("Location", String("http://hamhamhub/index.html"));
    request->send(response);
    //server.client().stop();
}

void send200OK(AsyncWebServerRequest *request)
{
    // note: we're 'cache busting' all of these responses so the browser re-requests the api every time
    AsyncWebServerResponse *response = request->beginResponse(200, "text/html", "OK");
    request->beginResponse(200);
    response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    response->addHeader("Pragma", "no-cache");
    response->addHeader("Expires", "-1");
    request->send(response);
    //server.client().stop();
}

String getContentType(String filename)
{
    if (filename.endsWith(".htm"))
        return "text/html";
    else if (filename.endsWith(".html"))
        return "text/html";
    else if (filename.endsWith(".css"))
        return "text/css";
    else if (filename.endsWith(".js"))
        return "application/javascript";
    else if (filename.endsWith(".png"))
        return "image/png";
    else if (filename.endsWith(".gif"))
        return "image/gif";
    else if (filename.endsWith(".jpg"))
        return "image/jpeg";
    else if (filename.endsWith(".ico"))
        return "image/x-icon";
    else if (filename.endsWith(".xml"))
        return "text/xml";
    else if (filename.endsWith(".pdf"))
        return "application/x-pdf";
    else if (filename.endsWith(".zip"))
        return "application/x-zip";
    else if (filename.endsWith(".gz"))
        return "application/x-gzip";
    return "text/plain";
}
