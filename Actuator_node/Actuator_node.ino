/**
   BLE communcation with one output characteristic with AES-GCM encryption.

   The BLE communcation is whitelisted to only allow connections with
   a set gateway device.

   The characteristic will check if the received data is valid.

   Harikesh Subramanian
*/
#include <BLEDevice.h>
#include <BLEServer.h>
#include <mbedtls/gcm.h>
#include <DHT.h>

//#define DEBUG

#define BLE_DEVICE_NAME "Local Node 2"
#define SERVICE_UUID "86df3990-4bdf-442e-8eb7-04bbd173e4a7"
#define CHAR_UUID_LED "8a7a1f1d-3cc0-4fe7-ab8a-d75fbcfb1a7b"
#define BLE_GATEWAY_ADDRESS "DC:A6:32:10:74:08"
#define BLE_AES_KEY "ponmlkjihgfedcba"
#define LED_PIN 25

mbedtls_gcm_context aes;
char* key = BLE_AES_KEY;
RTC_DATA_ATTR unsigned long int nonce = 0;
char *iv = new char[12];

BLEServer *pServer = NULL;
BLEService *pService;
BLECharacteristic *pCharLed;

bool deviceConnected = false;

unsigned long prevMillis;
unsigned long currentMillis;

int ledValue = 0;

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
      delay(500);
      pServer->startAdvertising();
#ifdef DEBUG
      Serial.println("BLE server advertising");
#endif
      deviceConnected = false;
    }
};

// Callbacks for input characteristic (led brightness)
class LedCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      char* rxValue = (char*) pCharacteristic->getData();
      unsigned char decoded[16];
      int auth = decrypt(rxValue, decoded);
      if (auth == 0) {
#ifdef DEBUG
        Serial.write(decoded, 16);
        Serial.println();
#endif
        int newValue = 0, i = 0;
        while (decoded[i] != ';') {
          newValue *= 10;
          int digit = (int)decoded[i] - (int)'0';
          if (digit < 0 || digit > 9) {
#ifdef DEBUG
            Serial.println("Invalid data - NAN");
#endif
            return;
          }
          newValue += digit;
          i++;
        }
        if (newValue > 4095) {
#ifdef DEBUG
          Serial.println("Invalid data - > 255");
#endif
          return;
        }
        ledValue = newValue;
#ifdef DEBUG
        Serial.print("New LED value = ");
        Serial.println(newValue);
#endif
      }
      else {
#ifdef DEBUG
        Serial.println("Auth error");
#endif
      }
    }
};

void setup() {
#ifdef DEBUG
  Serial.begin(115200);
#endif

  pinMode(LED_BUILTIN, OUTPUT);
  ledcSetup(0, 9000, 12);
  ledcAttachPin(LED_PIN, 0);

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

  pCharLed = pService->createCharacteristic(
               CHAR_UUID_LED,
               BLECharacteristic::PROPERTY_WRITE
             );
  pCharLed->setCallbacks(new LedCallbacks);

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
  ledcWrite(0, ledValue);
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
