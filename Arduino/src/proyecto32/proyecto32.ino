#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include "SPIFFS.h"

#define RST_PIN 22
#define SS_PIN 21

MFRC522 reader(SS_PIN, RST_PIN);
MFRC522::MIFARE_Key key;

File myFile;

String usersJson;
String Users[8];

bool estaAbierto = false;

bool funcionActiva = false;

int cantUsuarios = 6;

byte LecturaUID[4];
byte Usuarios[8][4];

//Servidor Web///////////////////////////////////////////////
/*const char* ssid = "AquiTampoco";
const char* password = "notengoidea";
*/

const char* ssid = "JoakoMi8";
const char* password = "qqqqqqqq";


const char* PARAM_INPUT_1 = "opcion";
const char* PARAM_INPUT_2 = "eliminar";

AsyncWebServer server(80);

const int cerradura = 15;

String processor(const String& var) {
  //Serial.println(var);
  if (var == "BUTTONPLACEHOLDER") {
    String buttons = "";
    String estadoPuerta = "Estado: ";
    if (estaAbierto == LOW) {
      estadoPuerta = "Estado: Cerrado";
    }
    else if (estaAbierto == HIGH) {
      estadoPuerta = "Estado: Abierto";
    }
    buttons += "<button class=\"button button3\" onclick=\"toggle()\" >" + estadoPuerta + "</button>";


    return buttons;
  }

  if (var == "FAVICON") {
    String favicon = "<div id=\"topright\"> <img src=\"https://i.ibb.co/kGJk3RK/CRABLock-logo.png\" height=100> </div>";

    return favicon;
  }

  if (var == "MOSTRARUSUARIOS") {
    String alert = "<br>";
    String alertUsers = "";

    for (int i = 0; i < cantUsuarios; i++) {
      alertUsers = Users[i];
      if(alertUsers.length() > 5){
      alertUsers.toUpperCase();
      alertUsers.replace("\"", "");
      alertUsers.replace(",", ", ");
      alertUsers.replace(":", ": ");
      alertUsers.replace("{", "");
      alertUsers.replace("}", "");

      alert += "<a>" + alertUsers + " &nbsp; </a>";
      alert += "<a style=\"color:#FF0000; font-size: 20px;\", onclick=\"elimUser(";
      alert += i+1;
      alert += " )\"> x </a>";
      alert += "<br>";
      alert += "<br>";
    }
}

    return alert;
  }



  return String();
}

void setup() {
  /*Users[0] = "{\"id\":1,\"pass\":\"1234\",\"UID\":[211,240,171,002]}";
  Users[1] = "{\"id\":2,\"pass\":\"4321\",\"UID\":[249,116,037,179]}";
  Users[2] = "{\"id\":3,\"pass\":\"1590\",\"UID\":[014,200,190,141]}";
  Users[3] = "{\"id\":4,\"pass\":\"1234\",\"UID\":[211,240,171,002]}";
  Users[4] = "{\"id\":5,\"pass\":\"4321\",\"UID\":[249,116,037,179]}";
  Users[5] = "{\"id\":6,\"pass\":\"1590\",\"UID\":[014,200,190,141]}";*/
  
  

  Serial.begin(9600);
  while (!Serial) continue;
  if (!SPIFFS.begin()) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }
  //Cerradura inicia cerrada
  pinMode(cerradura, OUTPUT);
  digitalWrite(cerradura, LOW);

  SPI.begin();
  reader.PCD_Init();
  delay(4);
  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;
  }
  
  if (!SD.begin()) {
    Serial.println("SD card initialization failed!");
    //return;
  }else{
    getSdInfo();
    Serial.println("Listo");
    }

  

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }
  //Mostrar IP
  Serial.println(WiFi.localIP());
  obtenerUsuarios(SD);
  iniciarServerWeb();
}

void loop() {
  if (!funcionActiva) {
    leerTarjeta();
  } else {
    agregarUsuario();
  }
}


void iniciarServerWeb() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/index.html", String(), false, processor);
  });

  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/style.css", "text/css");
  });

  server.on("/main-page.html", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/main-page.html", String(), false, processor);
  });

  server.on("/main-page.css", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/main-page.css", "text/css");
  });

  //////////////////////////////////////////////////////
  //Interpretacion HTTP
  // Send a GET request to <ESP_IP>/update?state=<inputMessage>
  server.on("/update", HTTP_GET, [] (AsyncWebServerRequest * request) {
    String inputMessage;
    String inputParam;
    // GET input1 value on <ESP_IP>/update?state=<inputMessage>
    if (request->hasParam(PARAM_INPUT_1)) {
      inputMessage = request->getParam(PARAM_INPUT_1)->value();
      inputParam = PARAM_INPUT_1;
      if (inputMessage.toInt() == 0) {
        Serial.println("Cerrar");
        estaAbierto = LOW;
        digitalWrite(cerradura, estaAbierto);
      } else if (inputMessage.toInt() == 1) {
        Serial.println("Abrir");
        estaAbierto = HIGH;
        digitalWrite(cerradura, estaAbierto);
      }
      else if (inputMessage.toInt() == 2) {
        Serial.print("Toggle - ");
        estaAbierto = !estaAbierto;
        digitalWrite(cerradura, estaAbierto);
        if (estaAbierto) {
          Serial.println("Abierto");
        } else {
          Serial.println("Cerrado");
        }

      }
      else if (inputMessage.toInt() == 3) {
        Serial.println("Agregar Usuario");
        agregarUsuario();
      }
      else if (inputMessage.toInt() == 4) {
        Serial.println("Eliminar usuario");
      }
    }
    else if (request->hasParam(PARAM_INPUT_2)) {
      inputMessage = request->getParam(PARAM_INPUT_2)->value();
      inputParam = PARAM_INPUT_2;
      
      Serial.println("Eliminar usuario " + inputMessage);
        
      }
    else {
      inputMessage = "No message sent";
      inputParam = "none";
    }
    //Serial.println(inputMessage);
    request->send(200, "text/plain", "OK");
  });
  // Start server
  server.begin();
}

void noAutorizado() {
  Serial.println("no estas autorizado");
}

void usuario () {
  Serial.println("Bienvenido Usuario");
  Serial.println("abierto");
  estaAbierto = true;
  digitalWrite(cerradura, estaAbierto);
  //delay(2000);
}

void admin() {
  Serial.println("Bienvenido administrador ");
  Serial.println("abierto");
  estaAbierto = true;
  digitalWrite(cerradura, estaAbierto);
}

void dentro () {
  Serial.println("saliendo ");
  estaAbierto = true;
  digitalWrite(cerradura, estaAbierto);
  //delay(2000);
}

void getSdInfo(){
  uint8_t cardType = SD.cardType();
  Serial.println();
  Serial.print("SD Card Type: ");
  if(cardType == CARD_MMC){
    Serial.println("MMC");
  } else if(cardType == CARD_SD){
    Serial.println("SDSC");
  } else if(cardType == CARD_SDHC){
    Serial.println("SDHC");
  } else {
    Serial.println("UNKNOWN");
  }
  
  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);
  }


void leerTarjeta() {
  if (!reader.PICC_IsNewCardPresent())
    return;

  if (!reader.PICC_ReadCardSerial())
    return;

  Serial.print("UID:");
  for (byte i = 0; i < reader.uid.size; i++) {
    if (reader.uid.uidByte[i] < 0x10) {
      Serial.print(" 0");
    }
    else {
      Serial.print(" ");
    }
    Serial.print(reader.uid.uidByte[i], HEX);
    LecturaUID[i] = reader.uid.uidByte[i];
  }

  Serial.print("\t");

  bool userEncontrado = false;

  for (int i = 0; i < cantUsuarios; i++) {
    if (comparaUID(LecturaUID, Usuarios[i])) {
      userEncontrado = true;
    }
  }
  if (userEncontrado) {
    usuario();
  } else {
    noAutorizado();
  }

  // Halt PICC
  reader.PICC_HaltA();
  // Stop encryption on PCD
  reader.PCD_StopCrypto1();

}


boolean comparaUID(byte lectura[], byte usuario[])
{
  for (byte i = 0; i < reader.uid.size; i++) {
    if (lectura[i] != usuario[i])
      return (false);
  }
  return (true);
}

void obtenerUsuarios(fs::FS &fs) {
  myFile = fs.open("/cards.txt");
  int count = 0;


  //Obtener usuarios guardados en la SD
  if (myFile) {
    while (myFile.available()) {
      String list = myFile.readStringUntil('\r');
      Serial.println(list);
      list.trim();
      Users[count] = list;
      count++;
    }


    myFile.close();
  } else {
    Serial.println("Error abriendo el archivo cards.txt");
  }
  if (count > 0) {
    cantUsuarios = count + 1;
  }
  else {
    cantUsuarios = 0;
  }

  //Imprimir usuarios obtenidos
  Serial.println("Usuarios:");
  for (int i = 0; i < cantUsuarios; i++) {
    Serial.println(Users[i]);
  }

  //Deserializar y guardar usuarios obtenidos
  for (int i = 0; i < cantUsuarios; i++) {
    parseJson(Users[i], i);
  }
}


void agregarUsuario() {

  if (!funcionActiva) {
    Serial.println("Agregar Usuario");
    Serial.println("Acerque la tarjeta a ingresar");

  }
  funcionActiva = true;
  delay(50);


  if (!reader.PICC_IsNewCardPresent())
    return;


  if (!reader.PICC_ReadCardSerial())
    return;


  Serial.print("UID:");
  for (byte i = 0; i < reader.uid.size; i++) {
    if (reader.uid.uidByte[i] < 0x10) {
      Serial.print(" 0");
    }
    else {
      Serial.print(" ");
    }
    Serial.print(reader.uid.uidByte[i], HEX);
    LecturaUID[i] = reader.uid.uidByte[i];
  }

  // Halt PICC
  reader.PICC_HaltA();
  // Stop encryption on PCD
  reader.PCD_StopCrypto1();

  Serial.print("\t");

  bool userEncontrado = false;
  for (int i = 0; i < cantUsuarios; i++) {
    if (comparaUID(LecturaUID, Usuarios[i])) {
      userEncontrado = true;
    }
  }
  if (userEncontrado) {
    Serial.println("La tarjeta ya esta en el sistema");
  } else {
    for (int i = 0; i < 4; i++) {
      Usuarios[cantUsuarios][i] = LecturaUID[i];
    }
    cantUsuarios++;
    guardarUsuario(SD ,(cantUsuarios), "0000", LecturaUID);
  }

  funcionActiva = false;

}


void guardarUsuario(fs::FS &fs, int jsonId, String jsonPass, byte jsonUID[4]) {

  myFile = fs.open("/cards.txt", FILE_APPEND);

  StaticJsonDocument<200> doc;

  String jsonString;

  doc["id"] = jsonId;
  doc["pass"] = jsonPass;

  JsonArray data = doc.createNestedArray("UID");
  for (int i = 0; i < 4; i++) {
    data.add(jsonUID[i]);
  }

  serializeJson(doc, jsonString);

  Serial.println("Usuario a guardar:");
  Serial.println(jsonString);

  if (myFile) {
    delay(50);
    myFile.println(jsonString);
    Serial.println("Guardando usuario en cards.txt...");
    delay(50);
    myFile.close();
    Serial.println("Guardado.");
  } else {
    Serial.println("Error abriendo cards.txt");
  }
}


void parseJson(String jsonToParse, int userID) {
  StaticJsonDocument<200> doc;

  //Deserializar Json
  DeserializationError error = deserializeJson(doc, jsonToParse);

  //Comprobar errores en la deserializacion
  if (error) {
    Serial.println("");
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return;
  }

  //Guardar usuario
  int jsonId = doc["id"];
  String jsonPass = doc["pass"];
  for (int i = 0; i < 4; i++) {
    Usuarios[userID][i] = doc["UID"][i];
  }

  /*
    //Imprimir usuario
    Serial.println("");
    Serial.print("ID: ");
    Serial.println(jsonId);
    Serial.print("PASS: ");
    Serial.println(jsonPass);
    Serial.print("UID: ");
    for (int i = 0; i < 4; i++) {
    Serial.print(Usuarios[userID][i],HEX);
    Serial.print(", ");
    }
  */
}
