unsigned char encImg[65535]; // Buffer to hold encrypted image data

void userActions(WiFiClient client) {
  if (client.connected()) {
    // Send the structure of the node at the start
    char structure[100] = "{\"type\":\"camera\", \"input-tags\":[\"pitch\",\"yaw\"], \"input-values\":[\"\",\"\"]}";
    //sprintf(structure, "{\"type\":\"camera\", \"input-tags\":[\"pitch\",\"yaw\"], \"input-values\":[\"\",\"\"]}", nonce++);
    int len = strlen(structure);
#ifdef DEBUG
    Serial.println(structure);
    Serial.print("Structure length : ");
    Serial.println(len);
#endif
    unsigned char encStruct[128];
    encrypt(userKey, structure, encStruct, len);

    client.write((const char*) encStruct, 12 + len + 16);

    // Update input values
    char updateStr[10];
    sprintf(updateStr, "%d|%d", servoPitchVal, servoYawVal);
    len = strlen(updateStr);
#ifdef DEBUG
    Serial.print(updateStr);
    Serial.print(" Update length : ");
    Serial.println(len);
#endif

    unsigned char encUpdate[38];
    encrypt(userKey, updateStr, encUpdate, len);

    client.print(12 + len + 16);
    client.print('|');
    client.write((const char*) encUpdate, 12 + len + 16);
  }
  while (client.connected()) {
    if (client.available()) {
      uint8_t encMsg[200], msg[173];
      int len = client.read(encMsg, 200);

      int msgLen = len - 12 - 16;
      int auth = decrypt((char*)userKey, encMsg, msg, msgLen);
      if (auth == 0) {
        msg[msgLen] = '\0';
#ifdef DEBUG
        Serial.print("Received message = ");
        Serial.println((char*)msg);
#endif

        // Set value request
        if (msg[0] == (int)'s' && msg[1] == (int)'e' && msg[2] == (int)'t') {
          int newValue = 0;
          switch (msg[4]) {
            case (int)'0': // Pitch servo value
              for (int i = 6; i < msgLen; i++) {
                if (msg[i] < (int)'0' || msg[i] > (int)'9')
                  break;
                newValue *= 10;
                newValue += (int)(msg[i] - (int)'0');
              }
              if (newValue > 180)
                newValue = 180;
              servoPitchVal = newValue;
#ifdef DEBUG
              Serial.print("New value of pitch servo = ");
              Serial.println(servoPitchVal);
#endif

              pitchServo.write(servoPitchVal);
              break;

            case (int)'1': // Yaw servo value
              for (int i = 6; i < msgLen; i++) {
                if (msg[i] < (int)'0' || msg[i] > (int)'9')
                  break;
                newValue *= 10;
                newValue += (int)(msg[i] - (int)'0');
              }
              if (newValue > 180)
                newValue = 180;
              servoYawVal = newValue;
#ifdef DEBUG
              Serial.print("New value of pitch servo = ");
              Serial.println(servoYawVal);
#endif
              yawServo.write(servoYawVal);
              break;

            default:
#ifdef DEBUG
              Serial.println("Invalid request from user");
#endif
              break;
          }

          // Update user with new values
          char updateStr[10];
          sprintf(updateStr, "%d|%d", servoPitchVal, servoYawVal);
          int updateLen = strlen(updateStr);
#ifdef DEBUG
          Serial.print(updateStr);
          Serial.print(" Update length : ");
          Serial.println(updateLen);
#endif

          unsigned char encUpdate[38];
          encrypt(userKey, updateStr, encUpdate, updateLen);

          client.print(12 + updateLen + 16);
          client.print('|');
          client.write((const char*) encUpdate, 12 + updateLen + 16);
        }
      }
      else {
#ifdef DEBUG
        Serial.println("Authentication error when decoding message");
#endif
      }
    }
    // Take Image
    camera_fb_t * fb = NULL;
    fb = esp_camera_fb_get();
    if (!fb) {
#ifdef DEBUG
      Serial.println("Camera capture failed");
#endif
    }
    else {
      int encImgLen = fb->len + 12 + 16;
      if (encImgLen <= 65535) {
        encrypt(userKey, (char*)fb->buf, encImg, fb->len);
#ifdef DEBUG
        Serial.println(encImgLen);
#endif
        client.print(encImgLen);
        client.print('|');
        client.write((const char*)encImg, encImgLen);
        delay(100);
      }
      else {
#ifdef DEBUG
        Serial.println("Image too large");
#endif
      }
    }
    esp_camera_fb_return(fb);
  }
  userKeySet = false;
#ifdef DEBUG
  Serial.println("User disconnected.");
#endif
}
