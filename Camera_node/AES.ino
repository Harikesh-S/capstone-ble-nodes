char* serverKey = "1234567890123456";
uint8_t userKey[16];
bool userKeySet = false;
mbedtls_gcm_context aes;
int nonce = 0;
char iv[12];

void encrypt(uint8_t* key, char *input, unsigned char *output, int msgLen) {;
  sprintf(iv, "%012d", nonce++);
  encrypt(key,input,output,msgLen,(unsigned char*)iv);
}

/** Function to encrypt input string using AES-GCM
   The function uses the global nonce variable to generate a unique iv for each encryption
   The output contains the iv(12 bits)|data|tag(16 bits)
*/
void encrypt(uint8_t* key, char *input, unsigned char *output, int msgLen, unsigned char* iv) {
  for (int i = 0; i < 12; i++) {
    output[i] = iv[i];
  }
  mbedtls_gcm_init(&aes);
  mbedtls_gcm_setkey(&aes, MBEDTLS_CIPHER_ID_AES, (const unsigned char*) key, 16 * 8);
  mbedtls_gcm_crypt_and_tag(&aes, MBEDTLS_GCM_ENCRYPT, msgLen, (const unsigned char*) iv, 12, NULL, 0, (const unsigned char*)input, output + 12, 16, output + 12 + msgLen);
//  int i;
//  for(i=0;i<12;i++) {
//    Serial.print(output[i]);
//    Serial.print(" ");
//  }
//   Serial.println();
//  for(;i<12+msgLen;i++) {
//    Serial.print(output[i]);
//    Serial.print(" ");
//  }
//   Serial.println();
//  for(;i<12+msgLen+16;i++) {
//    Serial.print(output[i]);
//    Serial.print(" ");
//  }
//   Serial.println();
  mbedtls_gcm_free(&aes);
}

/** Function used to decrypt input string using AES-GCM
   The input string is assumed to have the iv - first 12 bytes,
   the tag - last 16 bytes with the rest being the data
   Returns 0 if the message was authenticated
*/
int decrypt(char* key, uint8_t* input, unsigned char *output,int msgLen) {
  mbedtls_gcm_init(&aes);
  mbedtls_gcm_setkey(&aes, MBEDTLS_CIPHER_ID_AES, (const unsigned char*) key, 128);
  int ret = mbedtls_gcm_auth_decrypt(&aes, msgLen, (const unsigned char*) input, 12, NULL, 0, (const unsigned char*) input + 12 + msgLen, 16, (const unsigned char*) input + 12, output);
  mbedtls_gcm_free(&aes);
  return ret;
}
