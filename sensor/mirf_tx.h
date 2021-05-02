#ifndef _MIRF_TX_H_
#define _MIRF_TX_H_

#include <avr/io.h>

#define NRF2PIN

//SPI
#ifdef NRF2PIN
#define MOMI_PIN	PB0
#define SCK_PIN 	PB2

#define MIRF_CSN_HI {PORTB |= _BV(SCK_PIN); _delay_us(500);}
#define MIRF_CSN_LO {PORTB &= ~_BV(SCK_PIN); _delay_us(500);}
#else
#define MOSI_PIN	PB0
#define MISO_PIN 	PB1
#define SCK_PIN 	PB2
#define CSN_PIN     PB4

#define MIRF_CSN_HI     PORTB |=  _BV(CSN_PIN);
#define MIRF_CSN_LO     PORTB &= ~_BV(CSN_PIN);
#endif

// Mirf settings
#define MIRF_CH         13
#define MIRF_PAYLOAD    8
#define MIRF_CONFIG     ((1 << MASK_RX_DR) | (1 << EN_CRC) | (0 << CRCO))

extern void mirf_init();
extern void mirf_config();
extern void mirf_set_TADDR(uint8_t * adr);
extern void mirf_config_register(uint8_t reg, uint8_t value);
extern void mirf_read_register(uint8_t reg, uint8_t * value, uint8_t len);
extern void mirf_write_register(uint8_t reg, uint8_t * value, uint8_t len);
extern void mirf_send(uint8_t * value, uint8_t len);

#endif /* _MIRF_TX_H_ */
