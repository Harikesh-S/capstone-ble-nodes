unsigned char encImg[65535];


void userActions(WiFiClient client) {
  while ( client.connected()) {
    if (client.available()) {
    }
    // Take Image
    camera_fb_t * fb = NULL;
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
    }
    else {
      int encImgLen = fb->len + 12 + 16;
      if (encImgLen <= 65535) {
        Serial.println(fb->len);
        encrypt(userKey, (char*)fb->buf, encImg, fb->len);
        Serial.println(encImgLen);
        client.print(encImgLen);
        client.print('|');
        client.write((const char*)encImg, encImgLen);
        delay(100);
      }
      else {
        Serial.println("Image too large");
      }
    }
    esp_camera_fb_return(fb);
  }
  Serial.println("User disconnected.");
}
