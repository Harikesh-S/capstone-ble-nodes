/**
   BLE communcation with two sensor characteristics and one timer
   characteristic with AES-GCM encryption.

   The BLE communcation is whitelisted to only allow connections with
   a set gateway device.

   The sensor characteristics will be updated with new data when data is requested.
   The timer characteristic will check if the received data is valid.

   Harikesh Subramanian
*/
#include <BLEDevice.h>
#include <BLEServer.h>
#include <mbedtls/gcm.h>
#include <DHT.h>

// #define DEBUG

#define BLE_DEVICE_NAME "Local Node 1"
#define SERVICE_UUID "86df3990-4bdf-442e-8eb7-04bbd173e4a7"
#define CHAR_UUID_TEMP "1c70ab2e-c645-4853-b46a-fd4cd0b7f538"
#define CHAR_UUID_LIGHT "2a47596d-8402-4359-952a-a956c84b0f41"
#define CHAR_UUID_SLEEP "cac889a0-4436-489b-ba6c-0e4f9b2d47ca"
#define BLE_GATEWAY_ADDRESS "DC:A6:32:10:74:08"
#define BLE_AES_KEY "abcdefghijklmnop"
#define DHT_PIN 32
#define LDR_PIN 33

DHT dht(DHT_PIN, DHT11);

mbedtls_gcm_context aes;
char* key = BLE_AES_KEY;
RTC_DATA_ATTR unsigned long int nonce = 0;
char *iv = new char[12];

BLEServer *pServer = NULL;
BLEService *pService;
BLECharacteristic *pCharTemp;
BLECharacteristic *pCharLight;
BLECharacteristic *pCharSleep;

bool deviceConnected = false;
RTC_DATA_ATTR int sleepTime = 10;

unsigned long prevMillis;
unsigned long currentMillis;

float temperature = 22.7;
float humidity = 78;
float heatIndex = 23.0;
int light = 50;

/** Function to encrypt input string using AES-GCM
   The function uses the global nonce variable to generate a unique iv for each encryption
   The output contains the iv(12 bits)|data(16 bits)|tag(16 bits)
*/
void encrypt(char *input, unsigned char *output) {
  sprintf(iv, "%012d", nonce++);
  for (int i = 0; i < 12; i++) {
    output[i] = iv[i];
  }
  mbedtls_gcm_init(&aes);
  mbedtls_gcm_setkey(&aes, MBEDTLS_CIPHER_ID_AES, (const unsigned char*) key, 16 * 8);
  mbedtls_gcm_crypt_and_tag(&aes, MBEDTLS_GCM_ENCRYPT, 16, (const unsigned char*) iv, 12, NULL, 0, (const unsigned char*)input, output + 12, 16, output + 28);
  mbedtls_gcm_free(&aes);
}

/** Function used to decrypt input string using AES-GCM
   The input string is assumed to have the iv - first 12 bytes,
   the tag - last 16 bytes with the rest being the data
   Returns 0 if the message was authenticated
*/
int decrypt(char* input, unsigned char *output) {
  mbedtls_gcm_init(&aes);
  mbedtls_gcm_setkey(&aes, MBEDTLS_CIPHER_ID_AES, (const unsigned char*) key, 128);
  int ret = mbedtls_gcm_auth_decrypt(&aes, 16, (const unsigned char*) input, 12, NULL, 0, (const unsigned char*) input + 28, 16, (const unsigned char*) input + 12, output);
  mbedtls_gcm_free(&aes);
  return ret;
}

// Callbacks for the BLE Server
class ServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer *pServer) {
#ifdef DEBUG
      Serial.println("Device Connected!");
#endif
      deviceConnected = true;
      digitalWrite(LED_BUILTIN, HIGH);
      BLEDevice::setMTU(100);
    }
    void onDisconnect(BLEServer *pServer) {
#ifdef DEBUG
      Serial.println("Device Disconnected!");
#endif
      if (sleepTime == 0) {
        //delay(500);
        pServer->startAdvertising();
#ifdef DEBUG
        Serial.println("BLE server advertising");
#endif
      }
      else {
        esp_sleep_enable_timer_wakeup(sleepTime * 1000000);
#ifdef DEBUG
        Serial.print("Entering deep sleep for ");
        Serial.print(sleepTime);
        Serial.println(" seconds.");
        Serial.flush();
#endif
        esp_deep_sleep_start();
      }
      deviceConnected = false;
    }
};

// Callbacks for input characteristic (to get deep sleep time)
class SleepTimerCharacteristicCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      char* rxValue = (char*) pCharacteristic->getData();
      unsigned char decoded[16];
      int auth = decrypt(rxValue, decoded);
      if (auth == 0) {
#ifdef DEBUG
        Serial.write(decoded, 16);
        Serial.println();
#endif
        int newSleepTime = 0, i = 0;
        while (decoded[i] != ';') {
          newSleepTime *= 10;
          newSleepTime += (int)decoded[i] - (int)'0';
          i++;
        }
        sleepTime = newSleepTime;
#ifdef DEBUG
        Serial.print("New deep sleep time = ");
        Serial.println(newSleepTime);
#endif
      }
      else {
#ifdef DEBUG
        Serial.println("Auth error");
#endif
      }
    }
};

// Callback for temperature sensor data read
class TempSensorDataCharacteristicCallbacks: public BLECharacteristicCallbacks {
    void onRead(BLECharacteristic *pCharacteristic) {
      temperature = dht.readTemperature();
      humidity = dht.readHumidity();
      heatIndex = dht.computeHeatIndex(false);
      char strTemperature[5], strHumidity[5], strHeatIndex[5];
      dtostrf(temperature, -4, 1, strTemperature);
      dtostrf(humidity, -4, 1, strHumidity);
      dtostrf(heatIndex, -4, 1, strHeatIndex);
      char sensorData[16];
      unsigned char encryptedData[44];
      sprintf(sensorData, "%s;%s;%s;", strTemperature, strHumidity, strHeatIndex);
#ifdef DEBUG
      Serial.println(sensorData);
#endif
      encrypt(sensorData, encryptedData);
      pCharTemp->setValue((uint8_t*)((char*)encryptedData), 44);
    }
};

// Callback for light sensor data read
class LightSensorDataCharacteristicCallbacks: public BLECharacteristicCallbacks {
    void onRead(BLECharacteristic *pCharacteristic) {

      light = analogRead(LDR_PIN);
      char strLight[5];
      dtostrf(light, -4, 0, strLight);
      char sensorData[16];
      unsigned char encryptedData[44];
      sprintf(sensorData, "%s;", strLight);
#ifdef DEBUG
      Serial.println(sensorData);
#endif
      encrypt(sensorData, encryptedData);
      pCharLight->setValue((uint8_t*)((char*)encryptedData), 44);
    }
};

void setup() {
#ifdef DEBUG
  Serial.begin(115200);
#endif
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(LDR_PIN, INPUT);
  dht.begin();

#ifdef DEBUG
  Serial.println("Initializing BLE Server...");
#endif

  BLEDevice::init(BLE_DEVICE_NAME);
  BLEDevice::setMTU(100);
  BLEAddress gateway(BLE_GATEWAY_ADDRESS);
  BLEDevice::whiteListAdd(gateway);

  pServer = BLEDevice::createServer();
  pService = pServer->createService(SERVICE_UUID);
  pServer->setCallbacks(new ServerCallbacks());

  pCharLight = pService->createCharacteristic(
                 CHAR_UUID_LIGHT,
                 BLECharacteristic::PROPERTY_READ |
                 BLECharacteristic::PROPERTY_NOTIFY
               );
  pCharLight->setCallbacks(new LightSensorDataCharacteristicCallbacks);
  pCharTemp = pService->createCharacteristic(
                CHAR_UUID_TEMP,
                BLECharacteristic::PROPERTY_READ |
                BLECharacteristic::PROPERTY_NOTIFY
              );
  pCharTemp->setCallbacks(new TempSensorDataCharacteristicCallbacks);

  pCharSleep = pService->createCharacteristic(
                 CHAR_UUID_SLEEP,
                 BLECharacteristic::PROPERTY_WRITE
               );
  pCharSleep->setCallbacks(new SleepTimerCharacteristicCallbacks);

  pService->start();


  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->setScanFilter(false, true);
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();

#ifdef DEBUG
  Serial.println("BLE server advertising");
#endif

  prevMillis = millis();
}

void loop() {
  // Blink LED if no device is connected
  if (!deviceConnected) {
    currentMillis = millis();
    if (currentMillis - prevMillis > 500) {
      prevMillis = currentMillis;
      digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    }
  }
  delay(10);
}
