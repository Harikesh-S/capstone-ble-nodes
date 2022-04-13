void serverActions(WiFiClient client) {
  uint8_t data[200];
  unsigned char message[172];
  while (client.connected()) {
    if (client.available()) {
      int len = client.read(data, 200);
      
#ifdef DEBUG
      Serial.print("Received message. Length = ");
      Serial.print(len);
      Serial.println();
      for (int i = 0; i < len; i++) {
        Serial.print(data[i]);
        Serial.print(" ");
      }
      Serial.println();
#endif

      int msgLen = len - 12 - 16;
      int auth = decrypt(serverKey, data, message, msgLen);
      if (auth == 0) {

        if (message[12] == int('K') && message[13] == int('E') && message[14] == int('Y')) {
#ifdef DEBUG
          Serial.println("Key requested.");
#endif

          if (userKeySet) {
            // User is already connected, thus do not generate a new key
            break;
          }

          // Generate key, encrypt it, send it and disconnect
          userKeySet = true;
          int keyLen = 16;

          esp_random();

#ifdef DEBUG
          Serial.print("Generated key : ");
#endif
          for (int i = 0; i < keyLen; i++) {
            userKey[i] = random(256);
#ifdef DEBUG
            Serial.print(userKey[i]);
            Serial.print(" ");
#endif
          }
#ifdef DEBUG
          Serial.println();
#endif

          unsigned char encryptedKey[12 + 16 + keyLen];
          encrypt((uint8_t*)serverKey, (char*)userKey, encryptedKey, keyLen, message);
          client.write((const char*)encryptedKey, 12 + keyLen + 16);

          break;
        }
        else {
#ifdef DEBUG
          Serial.println("Unknown request.");
#endif
          break;
        }
      }
      else {
#ifdef DEBUG
        Serial.println("Authentication error");
#endif
      }
    }
  }
#ifdef DEBUG
  Serial.println("Server disconnected.");
#endif
}
