// ===== WEB SERVER (виправлено) =====
void startWebServer() {
  server.on("/", handleRoot);
  server.on("/api", HTTP_GET, handleAPI);
  server.on("/api", HTTP_POST, handleAPI);
  server.serveStatic("/style.css", LittleFS, "/style.css");
  server.serveStatic("/script.js", LittleFS, "/script.js");
  server.onNotFound([]() {
    String path = server.uri();
    String contentType = "text/html";
    if (path.endsWith(".css")) contentType = "text/css";
    if (path.endsWith(".js")) contentType = "application/javascript";
    
    if (!handleFile(path, contentType)) {
      server.send(404, "text/plain", "File not found");
    }
  });
  server.begin();
}

void handleRoot() {
  handleFile("/index.html", "text/html");
}

bool handleFile(String path, String contentType) {
  if (path.endsWith("/")) path += "index.html";
  
  if (LittleFS.exists(path)) {
    File file = LittleFS.open(path, "r");
    server.streamFile(file, contentType);
    file.close();
    return true;
  }
  return false;
}