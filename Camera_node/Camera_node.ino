#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include <esp_camera.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <mbedtls/gcm.h>
#include "mbedtls/pk.h";
#include "mbedtls/entropy.h";
#include "mbedtls/ctr_drbg.h";

#define DEBUG

#include "credentials.h"
#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"
#define PITCH_SERVO_PIN 14
#define YAW_SERVO_PIN 15
#define STATUS_LED 33

const char* ssid = SSID;
const char* password = PASSWORD;

WiFiServer serverTCP(50000);
WiFiServer userTCP(50001);

IPAddress local_IP(192, 168, 1, 8);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

mbedtls_pk_context pk;
mbedtls_entropy_context entropy;
mbedtls_ctr_drbg_context ctr_drbg;

int servoPitchVal = 0;
int servoYawVal = 0;

Servo servoN1; // Used to ignore two PWM channels
Servo servoN2;
Servo pitchServo;
Servo yawServo;

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // disable brownout detector

#ifdef DEBUG
  Serial.begin(115200);
#endif

  pitchServo.setPeriodHertz(50);
  yawServo.setPeriodHertz(50);

  servoN1.attach(2, 1000, 2000);
  servoN2.attach(13, 1000, 2000); // PWM channels used by the camera

  pitchServo.attach(PITCH_SERVO_PIN, 1000, 2000);
  yawServo.attach(YAW_SERVO_PIN, 1000, 2000);

  pitchServo.write(servoPitchVal);
  yawServo.write(servoYawVal);

  pinMode(STATUS_LED, OUTPUT);

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  // if PSRAM IC present, init with UXGA resolution and higher JPEG quality
  //                      for larger pre-allocated frame buffer.
  if (psramFound()) {
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
#ifdef DEBUG
    Serial.printf("Camera init failed with error 0x%x", err);
#endif
    return;
  }

  sensor_t * s = esp_camera_sensor_get();
  // initial sensors are flipped vertically and colors are a bit saturated
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1); // flip it back
    s->set_brightness(s, 1); // up the brightness just a bit
    s->set_saturation(s, -2); // lower the saturation
  }
  // drop down frame size for higher initial frame rate
  s->set_framesize(s, FRAMESIZE_HD);

  mbedtls_entropy_init(&entropy);
  mbedtls_ctr_drbg_init(&ctr_drbg);

  int ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, NULL, 0);
#ifdef DEBUG
  Serial.print("RNG generation status : ");
  Serial.println(ret);
#endif

  if (!WiFi.config(local_IP, gateway, subnet)) {
#ifdef DEBUG
    Serial.println("Failed to configure");
#endif
    while (true) {
      delay(10);
    }
  }

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);

    digitalWrite(STATUS_LED, !digitalRead(STATUS_LED));
#ifdef DEBUG
    Serial.print(".");
#endif
  }
#ifdef DEBUG
  Serial.println();

  Serial.print("WiFi connected, IP Address: ");
  Serial.println(WiFi.localIP());
#endif

  digitalWrite(STATUS_LED, LOW);

  userTCP.begin();
  serverTCP.begin();
}
void loop() {
  WiFiClient client = userTCP.available();
  if (client) {
#ifdef DEBUG
    Serial.println("User Connected");
#endif
    userActions(client);
  }

  client = serverTCP.available();
  if (client) {
#ifdef DEBUG
    Serial.println("Server Connected");
#endif
    serverActions(client);
  }
}
