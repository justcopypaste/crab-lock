#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include "SPIFFS.h"
#include <NTPClient.h>
#include <WiFiUdp.h>

#define RST_PIN 22
#define SS_PIN 21

MFRC522 reader(SS_PIN, RST_PIN);
MFRC522::MIFARE_Key key;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

File myFile;

String usersJson;
String Users[8];

bool estaAbierto = false;

bool funcionActiva = false;

int cantUsuarios = 6;

byte LecturaUID[4];
byte Usuarios[8][4];

//Servidor Web///////////////////////////////////////////////
/*const char* ssid = "Crablock";
  const char* password = "notengoidea";*/

const char* ssid = "Ceibal-2.4GHz";
const char* password = "";

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
      if (alertUsers.length() > 5) {
        alertUsers.toUpperCase();
        alertUsers.replace("\"", "");
        alertUsers.replace(",", ", ");
        alertUsers.replace(":", ": ");
        alertUsers.replace("{", "");
        alertUsers.replace("}", "");

        alert += "<a>" + alertUsers + " &nbsp; </a>";
        alert += "<a style=\"color:#FF0000; font-size: 20px;\", onclick=\"elimUser(";
        alert += i + 1;
        alert += " )\"> x </a>";
        alert += "<br>";
        alert += "<br>";
      }
    }

    return alert;
  }

  if (var == "MOSTRARLOGS") {
    String alert = "<br>";
    String log_line = "";

    for (int i = 0; i < 15; i++) {
      log_line = get_log_line(i);
      alert += "<a>" + log_line + " &nbsp; </a>";
      alert += "<br>";

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

  if (!SD.begin(5)) {
    Serial.println("SD card initialization failed!");
    //return;
  } else {
    // getSdInfo();
    Serial.println("Listo");
  }

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }
  //Mostrar IP
  Serial.println(WiFi.localIP());
  obtenerUsuarios();
  iniciarServerWeb();

  timeClient.begin();
  timeClient.setTimeOffset(-10800);

  showMenu();
}

void loop() {
  if (!funcionActiva) {
    leerTarjeta();
  } else {
    agregarUsuario();
  }

  if (Serial.available()) {
    String input = Serial.readString();
    int opc = input.toInt();
    switch (opc) {
      case 0:
        showMenu();
        break;
      case 1:
        crearUser();
        break;
      case 2:
        showUsers();
        break;
      case 3:
        delUser();
        break;
      case 4:
        listAllFiles();
        break;
      default:
        Serial.println("Opcion incorrecta");
        break;

    }
  }
  delay(50);
}
void showMenu() {
  Serial.println("Opciones:");
  Serial.println("0-Mostrar Menu");
  Serial.println("1-Agregar usuario");
  Serial.println("2-Mostrar usuarios");
  Serial.println("3-Eliminar usuario");
}

void crearUser() {
  Serial.println("Agregar usuario:");
  while (!Serial.available()) {
  }
  if (Serial.available()) {
    String input = Serial.readString();
    if (input.length() > 15) {
      saveString(input);
      Users[cantUsuarios] = input;
      String idUser = cantUsuarios + "";
      Serial.println("Usuario " + idUser + " agregado");
      cantUsuarios++;
    }
  }

}

void showUsers() {
  Serial.println("Usuarios:");
  for (int i = 0; i < cantUsuarios; i++) {
    if (Users[i].length() > 15) {
      Serial.println(i + Users[i]);
    }
  }
}

void delUser() {
  Serial.println("Eliminar usuario");
  Serial.println("Introduzca Id del usuario a eliminar:");
  while (!Serial.available()) {}
  if (Serial.available()) {
    String input = Serial.readString();
    int delId = input.toInt();
    if (delId > cantUsuarios) {
      Serial.println("Ese ID no existe");
      showMenu();
    } else {
      eliminarUsuario(delId);
    }

  }
}

void eliminarUsuario(int delId) {
  for (int i = delId; i < cantUsuarios; i++) {
    Users[i] = Users[i + 1];
    for (int x = 0; x < 4; x++) {
      Usuarios[i][x] = Usuarios[i + 1][x];
    }

  }
  cantUsuarios--;
  saveUsers();

  Serial.println("Usuario eliminado");
}

void listAllFiles() {

  File root = SPIFFS.open("/");

  File file = root.openNextFile();

  while (file) {

    Serial.print("FILE: ");
    Serial.println(file.name());

    file = root.openNextFile();
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

  server.on("/logs.txt", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SD, "/logs.txt", "text/css");
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
        Serial.println("Descargar logs");
        //download_logs(request);

        AsyncWebServerResponse *response = request->beginResponse(SPIFFS, "/users.txt", String(), true);
        response->addHeader("Content-Disposition", "attachment; filename=\"users.txt\"");

        request->send(response);
        //request->send(200, "text/plain", "OK");


      }
    }
    else if (request->hasParam(PARAM_INPUT_2)) {
      inputMessage = request->getParam(PARAM_INPUT_2)->value();
      inputParam = PARAM_INPUT_2;
      int inputId = inputMessage.toInt() - 1;
      Serial.println("Eliminar usuario " + inputId);
      eliminarUsuario(inputId);

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

String getDate() {
  String formattedDate;
  String dayStamp;
  String timeStamp;
  String date_time;

  while (!timeClient.update()) {
    timeClient.forceUpdate();
  }

  formattedDate = timeClient.getFormattedDate();
  // Extract date
  int splitT = formattedDate.indexOf("T");
  dayStamp = formattedDate.substring(0, splitT);
  // Extract time
  timeStamp = formattedDate.substring(splitT + 1, formattedDate.length() - 1);

  date_time = dayStamp + " " + timeStamp;
  return date_time;
}

void saveLog(String toSave) {
  File log_file;

  if (SD.exists("/logs.txt")) {
    log_file = SD.open("/logs.txt", FILE_APPEND);
  } else {
    log_file = SD.open("/logs.txt", FILE_WRITE);
  }

  String date_time = getDate();
  String log_line = date_time + " " + toSave;

  Serial.println(log_line);

  if (log_file) {
    delay(50);
    log_file.println(log_line);
    delay(50);
    log_file.close();
    Serial.println("log guardado en logs.txt");
  } else {
    Serial.println("error abriendo logs.txt");
  }
}

String get_log_line(int line) {
  File log_file = SD.open("/logs.txt");
  int count = 0;

  if (log_file) {
    while (log_file.available()) {
      String log_line = log_file.readStringUntil('\r');

      if (count == line) {
        return log_line;
      }
      count++;
    }
    return "";
  }
  else {
    return "Error abriendo logs.txt";
  }

}

void download_logs(AsyncWebServerRequest * request) {

  File download = SD.open("/logs.txt");
  if (download) {
    /* AsyncWebServerResponse *response = request->beginResponse(SD, download, String(), true);
      response->addHeader("Server", "ESP Async Web Server");
      request->send(response);*/
    download.close();
  } else Serial.println("error abriendo logs.txt");

}


void noAutorizado(byte lectura[4]) {
  Serial.println("no estas autorizado");
  String tarjeta;
  for (int i = 0; i < 4; i++) {
    tarjeta += String(lectura[i], HEX) + ":";
  }
  tarjeta.toUpperCase();

  String toSave = tarjeta + " Acceso denegado";
  saveLog(toSave);
}

void usuario (byte lectura[4]) {
  Serial.println("Bienvenido Usuario");
  Serial.println("abierto");
  estaAbierto = true;
  digitalWrite(cerradura, estaAbierto);
  //delay(2000);
  String tarjeta;
  for (int i = 0; i < 4; i++) {
    tarjeta += String(lectura[i], HEX) + ":";
  }
  tarjeta.toUpperCase();
  String toSave = tarjeta + " Acceso autorizado";
  saveLog(toSave);
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

  bool accesoAutorizado = false;

  for (int i = 0; i < cantUsuarios; i++) {
    if (comparaUID(LecturaUID, Usuarios[i])) {
      accesoAutorizado = true;
    }
  }
  if (accesoAutorizado) {
    usuario(LecturaUID);
  } else {
    noAutorizado(LecturaUID);
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


void obtenerUsuarios() {
  myFile = SPIFFS.open("/users.txt");
  int count = 0;

  //Obtener usuarios guardados en la SD
  if (myFile) {
    while (myFile.available()) {
      String list = myFile.readStringUntil('\r');
      //Serial.println(list);
      list.trim();
      if (list.length() > 5) {
        Users[count] = list;
        count++;
      }
    }
    myFile.close();

  } else {
    Serial.println("Error abriendo el archivo cards.txt");
  }

  if (count > 0) {
    cantUsuarios = count;
  }
  else {
    cantUsuarios = 0;
  }

  //Ajustar cantidad de usuarios obtenidos e imprimir usuarios
  int usuariosVacios = 0;
  //Serial.println("Usuarios:");
  for (int i = 0; i < cantUsuarios; i++) {
    if (Users[i].length() > 5) {
      //Serial.println(Users[i]);
      parseJson(Users[i], i);
    } else {
      usuariosVacios++;
    }
  }
  cantUsuarios = cantUsuarios - usuariosVacios;
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

    guardarUsuario(cantUsuarios, "0000", LecturaUID);
    cantUsuarios++;
  }

  funcionActiva = false;

}


void guardarUsuario(int jsonId, String jsonPass, byte jsonUID[4]) {

  myFile = SPIFFS.open("/users.txt", FILE_APPEND);

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

  Users[cantUsuarios] = jsonString;

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
    /*Serial.println("");
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());*/
    return;
  }

  //Guardar usuario
  int jsonId = doc["id"];
  String jsonPass = doc["pass"];
  for (int i = 0; i < 4; i++) {
    Usuarios[userID][i] = doc["UID"][i];
  }

}

void saveUsers() {
  if (SPIFFS.exists("/users.txt")) {
    if (SPIFFS.exists("/users_old.txt")) {
      SPIFFS.remove("/users_old.txt");
    }
    SPIFFS.rename("/users.txt", "/users_old.txt");
  }
  for (int i = 0; i < cantUsuarios; i++ ) {
    if (Users[i].length() > 5) {
      saveString(Users[i]);
    }
  }

}

void saveString(String datos) {
  if (SPIFFS.exists("/users.txt")) {
    myFile = SPIFFS.open("/users.txt", FILE_APPEND);
  } else {
    myFile = SPIFFS.open("/users.txt", FILE_WRITE);
  }
  StaticJsonDocument<200> doc;

  if (myFile) {
    delay(50);
    myFile.println(datos);
    //Serial.println("Guardando usuario en users.txt...");
    delay(50);
    myFile.close();
    //Serial.println("Guardado.");
  } else {
    Serial.println("Error abriendo users.txt");
  }
}
