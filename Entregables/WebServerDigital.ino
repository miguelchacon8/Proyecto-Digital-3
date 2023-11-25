/*************************************************************************************************
Curso: Digital 3
Programa: Servidor para control de LEDS por ESP32 via Wi-Fi
Creado por: Miguel Chacón, Oscar Donis y Luis Carranza
**************************************************************************************************/
//************************************************************************************************
// Librerías
//************************************************************************************************
#include <WiFi.h>
#include <WebServer.h>
//************************************************************************************************
// Variables globales
//************************************************************************************************
// SSID & Password
const char* ssid = "F";  // Enter your SSID here 
const char* password = "donas989";  //Enter your Password here
WebServer server(80);  // Object of WebServer(HTTP port, 80 is defult)
uint8_t LED1pin = 2;//LED ESP
//-----------PUERTOS UTILIZADOS EN ESP---------------
uint8_t LED2pin = 16; // LED1 UTR 1
uint8_t LED3pin = 17; // LED2 UTR 1
uint8_t LED4pin = 5; // LED1 UTR 2
uint8_t LED5pin = 6; // LED2 UTR 2
//---------- Valores de control LED -----------------
bool LED1status = LOW; //LED ESP
bool LED2status = LOW; // LED1 UTR 1
bool LED3status = LOW; // LED2 UTR 1
bool LED4status = LOW; // LED1 UTR 2
bool LED5status = LOW; // LED2 UTR 2
//************************************************************************************************
// Configuración
//************************************************************************************************
void setup() {
  Serial.begin(115200); //Inicializa el puerto serial
  Serial.println("Try Connecting to "); 
  Serial.println(ssid); //Print en el puerto serial
  pinMode(LED1pin, OUTPUT); //configura pin led 1 como salida
  pinMode(LED2pin, OUTPUT); //configura pin led 2 como salida
  pinMode(LED3pin, OUTPUT); //configura pin led 3 como salida
  // Connect to your wi-fi modem
  WiFi.begin(ssid, password);
  // Check wi-fi is connected to wi-fi network
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected successfully");
  Serial.print("Got IP: ");
  Serial.println(WiFi.localIP());  //Show ESP32 IP on serial
  server.on("/", handle_OnConnect); // Directamente desde e.g. 192.168.0.8
  server.on("/led1on", handle_led1on);  
  server.on("/led1off", handle_led1off);
  server.on("/led2on", handle_led2on);
  server.on("/led2off", handle_led2off);
  server.on("/led3on", handle_led3on);
  server.on("/led3off", handle_led3off);
  server.on("/led4on", handle_led4on);
  server.on("/led4off", handle_led4off);
  server.on("/led5on", handle_led5on);
  server.on("/led5off", handle_led5off);
  server.onNotFound(handle_NotFound);
  server.begin();
  Serial.println("HTTP server started");
  delay(100);
}
//************************************************************************************************
// loop principal
//************************************************************************************************
void loop() {
  server.handleClient();  //Maneja las peticiones del cliente
  if (LED1status){  //Si el estado de LED ESP32 es 1
    digitalWrite(LED1pin, HIGH);  //Enciende el LED ESP32
  }
  else{ //Si el estado de LED ESP32 es 0
    digitalWrite(LED1pin, LOW); //Apaga el LED ESP32
  }
  //LED2 - LED1 UTR 1
  if (LED2status){
    digitalWrite(LED2pin, HIGH);
  }
  else{
    digitalWrite(LED2pin, LOW);
  }
  //LED3 - LED2 UTR 1
  if (LED3status){
    digitalWrite(LED3pin, HIGH);
  }
  else{
    digitalWrite(LED3pin, LOW);
  }
  //LED4 - LED1 UTR 2
  if (LED4status){
    digitalWrite(LED4pin, HIGH);
  }
  else{
    digitalWrite(LED4pin, LOW);
  }
  //LED5 - LED2 UTR 2
  if (LED5status){
    digitalWrite(LED5pin, HIGH);
  }
  else{
    digitalWrite(LED5pin, LOW);
  }
}
//************************************************************************************************
// Handler de Inicio página
//************************************************************************************************
void handle_OnConnect() {
  LED1status = LOW; //Inicializa el estado de LED ESP32 en 0
  LED2status = LOW; //Inicializa el estado de LED1 UTR 1 en 0
  LED3status = LOW; //Inicializa el estado de LED2 UTR 1 en 0
  LED4status = LOW; //Inicializa el estado de LED1 UTR 2 en 0
  LED5status = LOW; //Inicializa el estado de LED2 UTR 2 en 0
  Serial.println("GPIO2 Status: OFF");
  Serial.println("LEDS APAGADAS");
  server.send(200, "text/html", SendHTML(LED1status,LED2status,LED3status,LED4status,LED5status));  //Envía el estado de los LEDS
}
//************************************************************************************************
// Handler de leds on
//************************************************************************************************
void handle_led1on() {  //Si se presiona el botón de encendido
  LED1status = HIGH;  //El estado de LED ESP32 es 1
  Serial.println("GPIO2 Status: ON");   //Imprime en el puerto serial
  server.send(200, "text/html", SendHTML(LED1status,LED2status,LED3status,LED4status,LED5status));  //Envía el estado de los LEDS
}
//LED2 - LED1 UTR 1
void handle_led2on() {  
  LED2status = HIGH;
  Serial.println("LED2 Status: ON");
  server.send(200, "text/html", SendHTML(LED1status,LED2status,LED3status,LED4status,LED5status));
}
//LED3 - LED2 UTR 1
void handle_led3on() {
  LED3status = HIGH;
  Serial.println("LED3 Status: ON");
  server.send(200, "text/html", SendHTML(LED1status,LED2status,LED3status,LED4status,LED5status));
}
//LED4 - LED1 UTR 2
void handle_led4on() {
  LED4status = HIGH;
  Serial.println("LED4 Status: ON");
  server.send(200, "text/html", SendHTML(LED1status,LED2status,LED3status,LED4status,LED5status));
}
//LED5 - LED2 UTR 2
void handle_led5on() {
  LED5status = HIGH;
  Serial.println("LED5 Status: ON");
  server.send(200, "text/html", SendHTML(LED1status,LED2status,LED3status,LED4status,LED5status));
}
//************************************************************************************************
// Handler de leds off
//************************************************************************************************
void handle_led1off() { //Si se presiona el botón de apagado
  LED1status = LOW; //El estado de LED ESP32 es 0
  Serial.println("GPIO2 Status: OFF");  //Imprime en el puerto serial
  server.send(200, "text/html", SendHTML(LED1status,LED2status,LED3status,LED4status,LED5status));  //Envía el estado de los LEDS
}
//LED2  - LED1 UTR 1
void handle_led2off() {
  LED2status = LOW;
  Serial.println("LED1 UTR1 Status: OFF");
  server.send(200, "text/html", SendHTML(LED1status,LED2status,LED3status,LED4status,LED5status));
}
//LED3 - LED2 UTR 1
void handle_led3off() {
  LED3status = LOW;
  Serial.println("LED2 UTR1 Status: OFF");
  server.send(200, "text/html", SendHTML(LED1status,LED2status,LED3status,LED4status,LED5status));
}
//LED4 - LED1 UTR 2
void handle_led4off() {
  LED4status = LOW;
  Serial.println("LED1 UTR2 Status: OFF");
  server.send(200, "text/html", SendHTML(LED1status,LED2status,LED3status,LED4status,LED5status));
}
//LED5 - LED2 UTR 2
void handle_led5off() {
  LED5status = LOW;
  Serial.println("LED2 UTR2 Status: OFF");
  server.send(200, "text/html", SendHTML(LED1status,LED2status,LED3status,LED4status,LED5status));
}
//************************************************************************************************
// Procesador de HTML
//************************************************************************************************
String SendHTML(uint8_t led1stat,uint8_t led2stat,uint8_t led3stat,uint8_t led4stat,uint8_t led5stat) {
  String ptr = "<!DOCTYPE html> <html>\n";
  ptr += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  ptr += "<title>LED Control</title>\n";
  ptr += "<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}\n";
  ptr += "body{margin-top: 50px;} h1 {color: #444444;margin: 50px auto 30px;} h3 {color: #444444;margin-bottom: 50px;}\n";
  ptr += ".button {display: block;width: 80px;background-color: #3498db;border: none;color: white;padding: 13px 30px;text-decoration: none;font-size: 25px;margin: 0px auto 35px;cursor: pointer;border-radius: 4px;}\n";
  ptr += ".button-on {background-color: #3498db;}\n";
  ptr += ".button-on:active {background-color: #2980b9;}\n";
  ptr += ".button-off {background-color: #34495e;}\n";
  ptr += ".button-off:active {background-color: #2c3e50;}\n";
  ptr += "p {font-size: 14px;color: #888;margin-bottom: 10px;}\n";
  ptr += "</style>\n";
  ptr += "</head>\n";
  ptr += " <link rel= stylesheet href= https://cdn.jsdelivr.net/npm/bootstrap@4.6.2/dist/css/bootstrap.min.css> \n";

  //BODY
  ptr += "<body>\n";
  ptr += "<h1>ESP32 Control de LEDS</h1>\n";
  ptr += "<h3>LEDS:</h3>\n";
  //LED1--------------------------------------------------------------------------------------------
  if (led1stat){
    ptr += "<p>LED ESP Status: ON</p><a class=\"button button-off\" href=\"/led1off\">OFF</a>\n";}
  else{
    ptr += "<p>LED ESP Status: OFF</p><a class=\"button button-on\" href=\"/led1on\">ON</a>\n";}
  //LED2----------------------------------------------------------------------------------------------
  if (led2stat){
    ptr += "<p>LED1-UTR1 Status: ON</p><a class=\"button button-off\" href=\"/led2off\">OFF</a>\n";}
  else{
    ptr += "<p>LED1-UTR1 Status: OFF</p><a class=\"button button-on\" href=\"/led2on\">ON</a>\n";}
  //LED3----------------------------------------------------------------------------------------------
  if (led3stat){
    ptr += "<p>LED2-UTR2 Status: ON</p><a class=\"button button-off\" href=\"/led3off\">OFF</a>\n";}
  else{
    ptr += "<p>LED2-UTR2 Status: OFF</p><a class=\"button button-on\" href=\"/led3on\">ON</a>\n";}
  //LED4----------------------------------------------------------------------------------------------
  if (led4stat){
    ptr += "<p>LED1-UTR2 Status: ON</p><a class=\"button button-off\" href=\"/led4off\">OFF</a>\n";}
  else{
    ptr += "<p>LED1-UTR2 Status: OFF</p><a class=\"button button-on\" href=\"/led4on\">ON</a>\n";}
  //LED2----------------------------------------------------------------------------------------------
  if (led5stat){
    ptr += "<p>LED2-UTR2 Status: ON</p><a class=\"button button-off\" href=\"/led5off\">OFF</a>\n";}
  else{
    ptr += "<p>LED2-UTR2 Status: OFF</p><a class=\"button button-on\" href=\"/led5on\">ON</a>\n";}

///-----------------------------------------------------------
  ptr += "</body>\n";
  ptr += "</html>\n";
  return ptr;
}
//************************************************************************************************
// Handler de not found
//************************************************************************************************
void handle_NotFound() {  //Si no se encuentra la página
  server.send(404, "text/plain", "Not found");  //Envía un mensaje de error
}
