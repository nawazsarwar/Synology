#include <stdio.h>
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/ssl.h"
int main(void){
  mbedtls_entropy_context e; mbedtls_ctr_drbg_context d; mbedtls_ssl_config c; mbedtls_ssl_context s;
  mbedtls_entropy_init(&e); mbedtls_ctr_drbg_init(&d); mbedtls_ssl_config_init(&c); mbedtls_ssl_init(&s);
  int r=mbedtls_ctr_drbg_seed(&d,mbedtls_entropy_func,&e,0,0);
  printf("seed=-0x%04x\n",(unsigned)-r);
  r=mbedtls_ssl_config_defaults(&c,MBEDTLS_SSL_IS_CLIENT,MBEDTLS_SSL_TRANSPORT_STREAM,MBEDTLS_SSL_PRESET_DEFAULT);
  printf("defaults=-0x%04x\n",(unsigned)-r);
  mbedtls_ssl_conf_rng(&c,mbedtls_ctr_drbg_random,&d);
  mbedtls_ssl_conf_authmode(&c,MBEDTLS_SSL_VERIFY_NONE);
  r=mbedtls_ssl_setup(&s,&c);
  printf("ssl_setup=-0x%04x\n",(unsigned)-r);
  return 0;
}
