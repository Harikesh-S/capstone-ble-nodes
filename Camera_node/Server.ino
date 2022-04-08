void serverActions(WiFiClient client) {
  uint8_t data[200];
  unsigned char message[172];
  while(client.connected()) {
    if(client.available()) {
      int len = client.read(data,200);
      Serial.print("Received message. Length = ");
      Serial.print(len);
      Serial.println();
      for(int i=0;i<len;i++) {
        Serial.print(data[i]);
        Serial.print(" ");
      }
      Serial.println();

      int msgLen = len - 12 - 16;
      int auth = decrypt(serverKey,data,message,msgLen);
      if(auth == 0) {
        int i;
        for(i=0; i<12; i++) {
          Serial.print((char)message[i]);
        }
        Serial.print("  ");
        for(;i<msgLen;i++) {
          Serial.print((char)message[i]);
        }
        Serial.println();
        if(message[12] == int('K') && message[13] == int('E') && message[14] == int('Y')) {
          Serial.println("Key requested.");
          // TODO Generate key
          char* generatedKey = "1234567890123456";
          int resMsgLen = 16;
          
          unsigned char toSend [12+16+resMsgLen];

          encrypt(serverKey, generatedKey, toSend, resMsgLen, message);
          int i;
          for(i=0;i<12;i++) {
            Serial.print(toSend[i]);
            Serial.print(" ");
          }
           Serial.println();
          for(;i<12+resMsgLen;i++) {
            Serial.print(toSend[i]);
            Serial.print(" ");
          }
           Serial.println();
          for(;i<12+resMsgLen+16;i++) {
            Serial.print(toSend[i]);
            Serial.print(" ");
          }

          client.write((const char*)toSend, 12+resMsgLen+16);
          break;
        }
        else {
          Serial.println("Unknown request.");
          break;
        }
      }
      else {
        Serial.println("Authentication error");
      }
    }
  }
  Serial.println("Server disconnected.");
}
