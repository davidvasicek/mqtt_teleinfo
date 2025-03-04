#include <WiFi.h>              // připojení knihovny WiFi; interní knihovna instalovaná s podporou čipu ESP32
#include <Wire.h>              // připojení knihovny Wire pro komunikaci I2C
#include <Adafruit_BME280.h>   // připojení knihovny Adafruit_BME280
#include <MQTTClient.h>        // připojení knihovny MQTTClient
#include <ArduinoJson.h>       // připojení knihovny ArduinoJson

#define SEALEVELPRESSURE_HPA (1013.25) // konstanta pro barometrický tlak na úrovni hladiny moře pro výpočet nadmořské výšky

#define BME280_ADRESA (0x76) //definice adresy BME280 senzoru

Adafruit_BME280 bme; // inicializace senzoru BME280 senzoru z knihovny

String pracoviste = "pracoviste_x"; // číslo pracoviště; nahraďte x číslem vašeho pracoviště
 
const char* ssid = "David-IoT";         // Název WiFi sítě; SSID
const char* password = "frydekmistek";   // Heslo WiFi sítě; password

unsigned long posledniCasOdeslani = 0; // inicializace proměnné posledniCasOdeslani typu long pro ukládání času posledního publikování

WiFiClient clientWiFi; // inicializace WiFi klienta
MQTTClient clientMQTT; // inicializace MQTT klienta

void setup(){
  
  Serial.begin(115200); // zahájení sériové komunikace s rychlostí 115200 baudů

  // inicializace BME280 senzoru; pokud není na dané adrese nalezen, vypíše o tom zprávu na monitoru a zacyklí se
  if (!bme.begin(BME280_ADRESA)){
    Serial.println("Could not find a valid BMP280 sensor, check wiring!");
    while (1);
  }

  delay(100); // vyčká 100 ms
  
  WiFi.mode(WIFI_STA); // nastavení WiFi modu jako STA (stanice)

  Serial.print("Připojuji se k síti: "); // výpis na obrazovku
  Serial.println(ssid); // výpis na obrazovku; název WiFi sítě

  WiFi.begin(ssid, password); // přihlášení k WiFi síti s údaji ssid a password

  // kontrola stavu WiFi; pokud není připojeno zacyklí se, dokud se spojení nenaváže
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }

  // pokud jsme připojeni pokračujeme dále výpisem na obrazovku potřebných informací
  Serial.println("");
  Serial.println("WiFi připojeno");
  Serial.print("Přidělená adresa: ");  
  Serial.println(WiFi.localIP()); // výpis na obrazovku; aktuální adresa zařízení

 Serial.println("Připojuji se k MQTT brokeru ..."); // výpis na obrazovku

 // inicializace objektu pomocí hostitelského jména zprostředkovatele, portu zprostředkovatelů (výchozí port 1883) a základní třídy Klient pro síťový přenos;
 // v případě šifrovaného přenosu definujte číslo portu clientMQTT.begin(const char hostname[], int port, Client &client);
 clientMQTT.begin("192.168.1.100", clientWiFi);

  // nastavení posluchače na odebírané téma; vše co do tématu pošlu, okamžitě opět přečtu; 
  // když přijde zpráva z odebíraného tématu, spustí se funkce messageReceived 
  clientMQTT.onMessage(messageReceived); 

  connectToMQTT(); // volání funkce pro připojení MQTT klienta
}

// výpis nové zprávy do sériového monitoru
void messageReceived(String &topic, String &payload) {
  
  Serial.println("příchozí: " + topic + " - " + payload);
}

// funkce pro připojení MQTT klienta
void connectToMQTT() {

  // připojení klienta ke zprostředkovateli pomocí dodaného ID klienta a volitelného uživatelského jména a hesla;
  // kanál try používá „try“ jako uživatelské jméno i heslo
  // připojování se zacyklí, dokud nebude klient připojen
  while (!clientMQTT.connect(pracoviste)) {
    Serial.print(".");
  }
 
 Serial.println("klient MQTT připojen"); // výpis na obrazovku
 
 clientMQTT.subscribe(pracoviste); // přihlášení se k odběru konkrétního tématu; budu odebírat BSS/pracoviste_x
}
 
void loop() {
  
  // Odesílá a přijímá pakety; tato funkce by měla být vyvolána v každém loop.
  clientMQTT.loop();

  // zkontroluji, jestli nedošlo k odhlášení klienta. Pokud ano, přihlas se znovu
  if(!clientMQTT.connected()) {
    
    connectToMQTT();
  }

  // jestliže je čas od posledního publikování zprávy větší než 5 sec, publikuj nové data
  if(millis() - posledniCasOdeslani > 5000) {

    float  teplota = bme.readTemperature(); // přečtu teplotu ze senzoru BME280 a uložím ji do proměnné inicializované proměnné teplota
    float  vlhkost = bme.readHumidity(); // přečtu vlhkost ze senzoru BME280 a uložím ji do proměnné inicializované proměnné vlhkost
    float  barometrickyTlak = bme.readPressure() / 100.0F; // přečtu barometrický tlak ze senzoru BME280 a uložím ji do proměnné inicializované proměnné barometrickyTlak
    float  nadmorskaVyska = bme.readAltitude(SEALEVELPRESSURE_HPA); // přečtu nadmořskou výšku ze sentoru BME280 a uložím ji do proměnné inicializované proměnné nadmorskaVyska
  
    StaticJsonBuffer<128> JsonBuffer; // vytvoření statického JSON bufferu o velikosti 128 bytů; možno vytvořit i buffer dynamicky 
    JsonObject& jsonObject = JsonBuffer.createObject(); // vytvoření nového JSON objektu
    
    jsonObject["Teplota"] = teplota; // vložení nové elementu Teplota do JSON objektu a přiřazení hodnoty teplota
    jsonObject["Vlhkost"] = vlhkost; // vložení nové elementu Vlhkost do JSON objektu a přiřazení hodnoty vlhkost
    jsonObject["BarometrickyTlak"] = barometrickyTlak; // vložení nové elementu BarometrickyTlak do JSON objektu a přiřazení hodnoty barometrickyTlak
    jsonObject["NadmorskaVyska"] = nadmorskaVyska; // vložení nové elementu NadmorskaVyska do JSON objektu a přiřazení hodnoty nadmorskaVyska
   
    String jsonString; // inicializace nové proměnné jsonString
    jsonObject.printTo(jsonString); // vytisknu jsonObjekt jako string do proměnné jsonString
    
    posledniCasOdeslani = millis();
    clientMQTT.publish(pracoviste, jsonString);  // publikování JSON zprávy do tématu BSS/pracoviste_x
  }
}
