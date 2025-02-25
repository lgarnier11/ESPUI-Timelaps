#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>

#include <Time.h>

#define DHTPIN 26 // Digital pin connected to the DHT sensor
// Feather HUZZAH ESP8266 note: use pins 3, 4, 5, 12, 13 or 14 --
// Pin 15 can work but DHT must be disconnected during program upload.

// Uncomment the type of sensor in use:
#define DHTTYPE DHT11 // DHT 11
// #define DHTTYPE DHT22 // DHT 22 (AM2302)
// #define DHTTYPE    DHT21     // DHT 21 (AM2301)

// See guide for details on sensor wiring and usage:
//   https://learn.adafruit.com/dht/overview

DHT_Unified dht(DHTPIN, DHTTYPE);

bool bMesure = true;
bool forceHotspot = true;
int seconds = 0, minutes = 0, hours = 0;
// Déclaration d'une variable globale pour stocker les messages reçus

// uint32_t delayMS;

#define REDPIN 5
#define BLUEPIN 4
int redSwitch = 15; // connexion bouton rouge
int heatPin = 13;   // connexion pour le chauffage

#define I2C_SDA 25
#define I2C_SCL 26

// LiquidCrystal_I2C lcd(0x27, 16, 2); // set the LCD address to 0x27 for a 16 chars and 2 line display

const long ONE_SECOND = 1000;

const char * TEMP_HEAT_ON = "temp_heatOn";
const char * TIMELAPS_DELAY = "delay_timelaps";
const char * TIMELAPS_DELAY_LIGHTON = "delay_lightOn_stored";

bool bReset = false;
bool bSettime = false;
bool bWifi = false;

// Web server==================================
#ifdef ESP8266
#include <ESP8266WiFi.h>
#elif defined ESP32
#include <WiFi.h>
#include <WiFiType.h>
#define LED_BUILTIN 2
#else
#error Architecture unrecognized by this code.
#endif

#include <Preferences.h>
Preferences preferences;
// #include <DNSServer.h>
#define DNS_PORT 53
IPAddress apIP(192, 168, 4, 1);
// DNSServer dnsServer;
const char *hostname = "ESPUI-Timelaps-1.1.2";
bool wificonnected = false;
// Web server==================================

// MQTT=========================
#include <PubSubClient.h>
WiFiClient espClient;
PubSubClient client(espClient);
#define mqtt_retry_delay 10000
unsigned long last_millis = 0;
// MQTT=========================

// ESPUI=================================================================================================================
#include <ESPUI.h>

uint16_t adr_cmd_reset;
uint16_t adr_cmd_settime;
// uint16_t timelaps_param_delay__text, timelaps_param_delay_lightOn_text;
uint16_t statusLabelId, serialLabelId;
uint16_t labelLog1, labelLog2, labelLog3, labelLog4;
String option;
//  ESPUI==================================================================================================================

class BaseParam
{
public:
    uint16_t message_adr_text;

    unsigned long last_millis;
    unsigned long delay;

    // Constructeur par défaut
    BaseParam() : message_adr_text(0), last_millis(0), delay(0) {}

};

class ParamTemp : public BaseParam
{
public:
    uint16_t temp_heatOn_adr_text;
    int temp_heatOn_stored;
    uint16_t heatOn_adr_text;
    ParamTemp() : temp_heatOn_adr_text(0), temp_heatOn_stored(0), heatOn_adr_text(0),  BaseParam() {}
};

class ParamTimelaps : public BaseParam
{
public:
    uint16_t delay_adr_text;
    int delay_stored;
    uint16_t delay_lightOn_adr_text;
    uint16_t delay_lightOn_stored;
    uint16_t lightOn_adr_text;
    uint16_t settime_adr_text;
    long nbLoop;
    long nbShoot;
    ParamTimelaps() : nbLoop(0), nbShoot(0), delay_stored(0), delay_adr_text(0), delay_lightOn_adr_text(0), delay_lightOn_stored(0), lightOn_adr_text(0), settime_adr_text(0), BaseParam() {}
};
// Déclaration des instances de Param
ParamTemp param_temp;
ParamTimelaps param_timelaps;

void updateTime() {
    seconds++;

    if (seconds >= 60) { seconds = 0; minutes++; }
    if (minutes >= 60) { minutes = 0; hours++; }
    if (hours >= 24)   { hours = 0; }

}

void getTime(char *buffer, size_t bufferSize)
{
    updateTime();  
    if (bufferSize >= 9) {  // Vérification pour éviter un dépassement de buffer
        snprintf(buffer, bufferSize, "%02d:%02d:%02d", hours, minutes, seconds);
    }
}

// const char* getTime() {
//     updateTime();
//     static char buffer[9];  // Utilisation de static pour éviter que le buffer ne soit détruit après la fin de la fonction
//     snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d", hours, minutes, seconds);
//     Serial.println(buffer);
//     return buffer;
// }

// Default ESPUI callback======================
void textCallback(Control *sender, int type)
{
}

// gestion du défilement de l'affichage
int maxLog = 0;
int maxTempMessage = 0;

const int MAX_TEMP_MESSAGE = 3;
void println(const char* s) {
    Serial.println(s);
}

void println(int i) {
    char buffer[12];  // Un entier signé 32 bits peut avoir jusqu'à 11 caractères (-2147483648) + '\0'
    snprintf(buffer, sizeof(buffer), "%d", i);
    Serial.println(buffer);
}

void println() {
    Serial.println();
}

void print(const char* s) {
    Serial.print(s);
}

void print(int i) {
    char buffer[12];
    snprintf(buffer, sizeof(buffer), "%d", i);
    Serial.print(buffer);
}

bool isNumeric(const String &str) {
    if (str.length() == 0) return false;  // Vérifier que la chaîne n'est pas vide

    for (int i = 0; i < str.length(); i++) {
        if (!isDigit(str[i])) return false;  // Vérifier que chaque caractère est un chiffre
    }

    return true;
}

float getTemperature()
{
    float temperature = 0;
    sensors_event_t event;
    dht.temperature().getEvent(&event);
    if (isnan(event.temperature))
    {
        Serial.println(F("Error reading temperature!"));
    }
    else
    {
        return event.temperature;
    }
    return -999.0;
}

// WiFi settings callback=====================================================
void SaveWifiDetailsCallback(Control *sender, int type)
{
    if (type == B_UP)
    {
        println("Saving params");

        param_temp.temp_heatOn_stored = ESPUI.getControl(param_temp.temp_heatOn_adr_text)->value.toInt();
        param_timelaps.delay_stored = ESPUI.getControl(param_timelaps.delay_adr_text)->value.toInt();
        param_timelaps.delay_lightOn_stored = ESPUI.getControl(param_timelaps.delay_lightOn_adr_text)->value.toInt();

        preferences.putInt(TEMP_HEAT_ON, param_temp.temp_heatOn_stored);
        preferences.putInt(TIMELAPS_DELAY, param_timelaps.delay_stored);
        preferences.putInt(TIMELAPS_DELAY_LIGHTON, param_timelaps.delay_lightOn_stored);
        println(param_temp.temp_heatOn_stored);
        println(param_timelaps.delay_stored);
        println(param_timelaps.delay_lightOn_stored);

        println("Saving settings");
    }
}
// WiFi settings callback=====================================================

// CMD MESURER=================================
void cmdResetCallback(Control *sender, int type)
{
    if (!bReset && !bSettime && !bWifi) {
        bReset = true;
        param_timelaps.nbLoop = param_timelaps.delay_stored - (param_timelaps.delay_lightOn_stored / 2) - 5;
        Serial.printf("Valeur reset : %d\n", param_timelaps.nbLoop);
        bReset = false;
    }
}
// void cmdSettimeCallback(Control *sender, int type)
// {   
//     if (!bReset && !bSettime && !bWifi) {
//         bSettime = true;
//         Control * control = ESPUI.getControl(param_timelaps.settime_adr_text);
//         String inputValue = control->value;
//         if (isNumeric(inputValue)) {
//             int num = inputValue.toInt();  // Convertir en entier

//             // Vérifier si c'est bien dans l'intervalle [x, y]
//             if (num >= 0 && num <= param_timelaps.delay_stored) {
//                 param_timelaps.nbLoop = num;
//                 Serial.printf("Valeur acceptée : %d\n", param_timelaps.nbLoop);
//             } else {
//                 String message = "Erreur : valeur hors limite ! (0, " + String(param_timelaps.delay_stored) + ")";
//                 Serial.println(message);
//                 ESPUI.print(param_timelaps.settime_adr_text, message);
//             }
//         } else {
//             Serial.println("Erreur : entrée non numérique !");
//             ESPUI.print(param_timelaps.settime_adr_text, "Entrée invalide !");
//         }
//         bSettime = false;
//     }
// }
void cmdSettimeCallback(Control *sender, int type)
{   
    if (!bReset && !bSettime && !bWifi) {
        bSettime = true;
        Control *control = ESPUI.getControl(param_timelaps.settime_adr_text);
        const char *inputValue = control->value.c_str(); // Conversion en `char*`
        
        if (isNumeric(inputValue)) {
            int num = atoi(inputValue);  // Convertir en entier

            // Vérifier si c'est bien dans l'intervalle [0, param_timelaps.delay_stored]
            if (num >= 0 && num <= param_timelaps.delay_stored) {
                param_timelaps.nbLoop = num;
                Serial.printf("Valeur acceptée : %d\n", param_timelaps.nbLoop);
            } else {
                char message[50];  
                snprintf(message, sizeof(message), "Erreur : valeur hors limite ! (0, %d)", param_timelaps.delay_stored);
                Serial.println(message);
                ESPUI.print(param_timelaps.settime_adr_text, message);
            }
        } else {
            Serial.println("Erreur : entrée non numérique !");
            ESPUI.print(param_timelaps.settime_adr_text, "Entrée invalide !");
        }
        bSettime = false;
    }
}

// CMD MESURER=================================

// ESPUI=====================================================================================================================
void espui_init()
{
    ESPUI.setVerbosity(Verbosity::Quiet);

    // Custom UI..............................................................................
    // Your code HERE !
    //  auto demo_tab = ESPUI.addControl(Tab, "", "demo");
    //  auto demo_button = ESPUI.addControl(Button, "", "Button", None, demo_tab, textCallback);
    // Custom UI..............................................................................

    // Mesure-------------------------------------------------------------------------------------------------------------------
    auto mesuretab = ESPUI.addControl(Tab, "", "Mesures");
    param_temp.message_adr_text = ESPUI.addControl(Label, "Température", "", Peterriver, mesuretab, textCallback);
    param_temp.heatOn_adr_text = ESPUI.addControl(Label, "Chauffage", "", Peterriver, mesuretab, textCallback);
    param_timelaps.message_adr_text = ESPUI.addControl(Label, "Timelaps", "", Peterriver, mesuretab, textCallback);
    param_timelaps.lightOn_adr_text = ESPUI.addControl(Label, "Lumière", "", Peterriver, mesuretab, textCallback);
    adr_cmd_reset = ESPUI.addControl(Button, "", "Reset timelaps", Peterriver, mesuretab, cmdResetCallback);
    param_timelaps.settime_adr_text = ESPUI.addControl(Text, "Définir le timer", "0", Peterriver, mesuretab, textCallback);
    adr_cmd_settime = ESPUI.addControl(Button, "", "Reset timer", Peterriver, mesuretab, cmdSettimeCallback);
    ESPUI.setEnabled(adr_cmd_reset, true);
    ESPUI.setEnabled(adr_cmd_settime, true);
    // Mesure-------------------------------------------------------------------------------------------------------------------

    // Param-----------------------------------------------------------------------------------------------
    auto param = ESPUI.addControl(Tab, "", "Paramétrage");

    param_temp.temp_heatOn_adr_text = ESPUI.addControl(Text, "Température de déclenchement du chauffage", String(param_temp.temp_heatOn_stored), Peterriver, param, textCallback);
    param_timelaps.delay_adr_text = ESPUI.addControl(Text, "Timelaps (délai entre chaque photo)", String(param_timelaps.delay_stored), Peterriver, param, textCallback);
    param_timelaps.delay_lightOn_adr_text = ESPUI.addControl(Text, "Délai allumage lumière", String(param_timelaps.delay_lightOn_stored), Peterriver, param, textCallback);

    auto paramsave = ESPUI.addControl(Button, "Save", "Save", Peterriver, param, SaveWifiDetailsCallback);
    // auto espreset = ESPUI.addControl(Button, "", "Reboot ESP", None, paramsave, ESPReset);

    ESPUI.setEnabled(param_temp.temp_heatOn_adr_text, true);
    ESPUI.setEnabled(param_timelaps.delay_adr_text, true);
    ESPUI.setEnabled(param_timelaps.delay_lightOn_adr_text, true);
    ESPUI.setEnabled(paramsave, true);
    // ESPUI.setEnabled(espreset, true);
    // Param-----------------------------------------------------------------------------------------------

    ESPUI.begin(hostname);
}
// ESPUI=====================================================================================================================

// WiFi================================================================================
void wifi_init()
{
    if (!bReset && !bSettime && !bWifi) {
        bWifi = true;
        delay(2000);
        print("WiFi.status() : ");
        print(WiFi.status());
        print(" WL_CONNECTED : ");
        println(WL_CONNECTED);
        if (forceHotspot || WiFi.status() != WL_CONNECTED)
        {
            wificonnected = false;
            if (forceHotspot)
            {
                print("(FORCE) ");
            }
            println("Creating Hotspot");

            WiFi.mode(WIFI_AP);
            delay(100);
            WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
            WiFi.softAP(hostname);
            delay(1000);
        }
        // dnsServer.start(DNS_PORT, "*", apIP);
        delay(100);
        print("WiFi.getMode : ");
        print(WiFi.getMode());
        print(" WIFI_AP : ");
        println(WIFI_AP);

        print("\nIP address : ");
        println(WiFi.getMode() == WIFI_AP ? WiFi.softAPIP() : WiFi.localIP());
        bWifi = false;
    }
}
// WiFi================================================================================

// Custom libraries..............

// SETUP=========================
void setup()
{
    Serial.begin(115200);
    while (!Serial)
        ; // Attendre que la connexion série soit établie

    // Initialisation des valeurs
    param_temp = ParamTemp();
    param_timelaps = ParamTimelaps();

    pinMode(REDPIN, OUTPUT);
    pinMode(BLUEPIN, OUTPUT);
    pinMode(redSwitch, INPUT);
    pinMode(heatPin, OUTPUT);
    // LCD 1602
    // lcd.init(I2C_SDA, I2C_SCL); // initialize the lcd to use user defined I2C pins
    // lcd.backlight();
    
    // lcd.setCursor(0, 1);
    // lcd.print("Temperature:");

    preferences.begin("Settings");

    // println("Time : " + getCurrentTime());
    wifi_init();
    // Custom setup...............

    // Initialize device.
    dht.begin();
    // Print temperature sensor details.
    sensor_t sensor;
    dht.temperature().getSensor(&sensor);

    param_temp.temp_heatOn_stored = preferences.getInt(TEMP_HEAT_ON, 23);

    param_timelaps.delay_stored = preferences.getInt(TIMELAPS_DELAY, 600);
    param_timelaps.delay_lightOn_stored = preferences.getInt(TIMELAPS_DELAY_LIGHTON, 60);
    // Custom setup...............
    espui_init();
    esp_reset_reason_t resetReason = esp_reset_reason();
    
    Serial.printf("Raison du dernier redémarrage : %d\n", resetReason);
    
    if (resetReason == ESP_RST_PANIC) {
        // on eteint lumière et chauffage
        ESPUI.print(param_temp.heatOn_adr_text, "Eteint");
        digitalWrite(heatPin, LOW);

        digitalWrite(BLUEPIN, LOW); // Light blue LED up
        digitalWrite(REDPIN, HIGH); // Turn red LED off
        ESPUI.print(param_timelaps.lightOn_adr_text, "Eteint");
        println("Eteint");

        Serial.println(F("Redémarrage dû à une erreur système !"));
        while (true) delay(1000);  // Empêcher un nouveau redémarrage
    }
}
// SETUP=========================

// LOOP==========================================
void loop()
{
    unsigned long currentMillis = millis(); // Temps actuel en millisecondes

    if (currentMillis - param_temp.last_millis >= ONE_SECOND && !bReset && !bSettime && !bWifi)
    {
        float fTemperature = getTemperature();
        if (fTemperature != -999.0) {
            char sTemperature[10];  
            snprintf(sTemperature, sizeof(sTemperature), "%.2f", fTemperature);

            if (fTemperature >= param_temp.temp_heatOn_stored)
            {
                ESPUI.print(param_temp.heatOn_adr_text, "Eteint");
                digitalWrite(heatPin, LOW);
            }
            else
            {
                ESPUI.print(param_temp.heatOn_adr_text, "Allumé");
                digitalWrite(heatPin, HIGH);
            }

            char currentTime[9];
            getTime(currentTime, sizeof(currentTime)); // Stocke l'heure actuelle dans `currentTime`

            char message[64];
            snprintf(message, sizeof(message), "Temperature: %s C (%d) %s", sTemperature, param_temp.temp_heatOn_stored, currentTime);
            
            preferences.putString("TIME", currentTime);
            println(message);
            ESPUI.print(param_temp.message_adr_text, message);

            param_temp.last_millis = currentMillis;
        }
    }

    if (currentMillis - param_timelaps.last_millis >= ONE_SECOND && !bReset && !bSettime && !bWifi)
    {
        Serial.println(ESP.getFreeHeap()); 

        param_timelaps.nbLoop++;

        if (param_timelaps.nbLoop <= param_timelaps.delay_lightOn_stored / 2 || param_timelaps.nbLoop >= (param_timelaps.delay_stored - param_timelaps.delay_lightOn_stored / 2))
        {
            digitalWrite(BLUEPIN, HIGH); 
            digitalWrite(REDPIN, LOW);   
            ESPUI.print(param_timelaps.lightOn_adr_text, "Allumé");
            println("Allumé");
        }
        else
        {
            digitalWrite(BLUEPIN, LOW); 
            digitalWrite(REDPIN, HIGH); 
            ESPUI.print(param_timelaps.lightOn_adr_text, "Eteint");
            println("Eteint");
        }

        if (param_timelaps.nbLoop >= param_timelaps.delay_stored)
        {
            param_timelaps.nbShoot++;
            param_timelaps.nbLoop = 0;
        }

        char message[64];
        snprintf(message, sizeof(message), "Timelaps: %d / %d - Photos:%d", param_timelaps.nbLoop, param_timelaps.delay_stored, param_timelaps.nbShoot);

        println(message);
        ESPUI.print(param_timelaps.message_adr_text, message);

        param_timelaps.last_millis = currentMillis;
    }
}

// void loop()
// {
//     // Custom loop.................................
//     // mise a jout toutes les secondes
//     unsigned long currentMillis = millis(); // Temps actuel en millisecondes
//     if (currentMillis - param_temp.last_millis >= ONE_SECOND && !bReset && !bSettime && !bWifi)
//     {
//         float fTemperature = getTemperature();
//         if (fTemperature != -999.0) {
//             String sTemperature = String(fTemperature);

//             if (fTemperature >= param_temp.temp_heatOn_stored)
//             {
//                 ESPUI.print(param_temp.heatOn_adr_text, "Eteint");
//                 digitalWrite(heatPin, LOW);
//             }
//             else
//             {
//                 ESPUI.print(param_temp.heatOn_adr_text, "Allumé");
//                 digitalWrite(heatPin, HIGH);
//             }
//             String message = "Temperature: " + sTemperature + " C (" + param_temp.temp_heatOn_stored + ") " + getTime();
//             preferences.putString("TIME", getTime());
//             println(message);
//             ESPUI.print(param_temp.message_adr_text, message);
//             param_temp.last_millis = currentMillis;
//         }
//     }
//     if (currentMillis - param_timelaps.last_millis >= ONE_SECOND && !bReset && !bSettime && !bWifi)
//     {
//         Serial.println(ESP.getFreeHeap()); 
//         param_timelaps.nbLoop++;
//         // lcd.setCursor(0, 0);
//         if (param_timelaps.nbLoop <= param_timelaps.delay_lightOn_stored / 2 || param_timelaps.nbLoop >= (param_timelaps.delay_stored - param_timelaps.delay_lightOn_stored / 2))
//         {
//             digitalWrite(BLUEPIN, HIGH); // Light blue LED up
//             digitalWrite(REDPIN, LOW);   // Turn red LED off
//             ESPUI.print(param_timelaps.lightOn_adr_text, "Allumé");
//             println("Allumé");
//         }
//         else
//         {
            
//             digitalWrite(BLUEPIN, LOW); // Light blue LED up
//             digitalWrite(REDPIN, HIGH); // Turn red LED off
//             ESPUI.print(param_timelaps.lightOn_adr_text, "Eteint");
//             println("Eteint");
            
//         }
//         if (param_timelaps.nbLoop >= param_timelaps.delay_stored)
//         {
//             param_timelaps.nbShoot++;
//             param_timelaps.nbLoop = 0;
//         }

//         String message = "Timelaps: " + String(param_timelaps.nbLoop) + " / " + param_timelaps.delay_stored + " - Photos:" + param_timelaps.nbShoot;
//         println(message);
//         ESPUI.print(param_timelaps.message_adr_text, message);
//         param_timelaps.last_millis = currentMillis;
//     }

//     // Custom loop.................................
// }
// LOOP==========================================
