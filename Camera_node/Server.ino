void serverActions(WiFiClient client) {
  uint8_t data[1000];
  unsigned char message[972];
  while (client.connected()) {
    if (client.available()) {
      int len = client.read(data, 1000);

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

        message[msgLen] = '\0';

        if (message[0] == int('K') && message[1] == int('E') && message[2] == int('Y')) {
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

          unsigned char encryptedKey[MBEDTLS_MPI_MAX_SIZE];
          size_t olen = 0;

          mbedtls_pk_init(&pk);

          int pkLen = msgLen - 3 + 1; // - Req, + 1 null character
          unsigned char* publicKey = message + 3;


          int ret = mbedtls_pk_parse_public_key(&pk, (const unsigned char*)(publicKey), pkLen);
#ifdef DEBUG
          Serial.print("Public Key read status : ");
          Serial.println(ret);
#endif

          ret = mbedtls_pk_encrypt(&pk, (const unsigned char*)userKey, keyLen, encryptedKey, &olen, sizeof(encryptedKey), mbedtls_ctr_drbg_random, &ctr_drbg);
#ifdef DEBUG
          Serial.print("RSA Encryption status : ");
          Serial.println(ret);
#endif

          if (ret == 0) {
#ifdef DEBUG
            Serial.println(olen);
#endif
            client.write((const char*)encryptedKey, olen);
          }

          mbedtls_pk_free(&pk);

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
