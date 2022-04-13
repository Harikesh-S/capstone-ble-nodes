unsigned char encImg[65535]; // Buffer to hold encrypted image data

void userActions(WiFiClient client) {
  if(client.connected()) {
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
    
    client.write((const char*) encStruct, 12+len+16);

    // Update input values
    char updateStr[10];
    sprintf(updateStr, "%d|%d", servoPitch, servoYaw);
    len = strlen(updateStr);
    #ifdef DEBUG
    Serial.print(updateStr);
    Serial.print(" Update length : ");
    Serial.println(len);
    #endif
    
    unsigned char encUpdate[38];
    encrypt(userKey, updateStr, encUpdate, len);
    
    client.print(12+len+16);
    client.print('|');
    client.write((const char*) encUpdate, 12+len+16);
  }
  while (client.connected()) {
    if (client.available()) {
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
