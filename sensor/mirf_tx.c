#include <util/delay.h>
#include "mirf_tx.h"
#include "nrf24l01.h"

#include <avr/io.h>
#include <avr/interrupt.h>

#define TX_POWERUP mirf_config_register(CONFIG, MIRF_CONFIG | _BV(PWR_UP))

#ifdef NRF2PIN
inline void spi_init()
{
    DDRB |= _BV(MOMI_PIN) | _BV(SCK_PIN);
    PORTB |= _BV(SCK_PIN);
    PORTB &= ~_BV(MOMI_PIN);
}

uint8_t spi_send(uint8_t c)
{
    uint8_t datain, bits = 8;

    do {
        datain <<= 1;
        if(PINB & _BV(MOMI_PIN)) {datain++;}
        DDRB |= _BV(MOMI_PIN); // output mode
        if (c & 0x80) {PORTB |= _BV(MOMI_PIN);}

        PORTB |= _BV(SCK_PIN);
        PORTB &= ~_BV(SCK_PIN);

        PORTB &= ~_BV(MOMI_PIN);
        DDRB &= ~_BV(MOMI_PIN); // input mode

        c <<= 1;

    } while(--bits);

    return datain;
}
#else
inline void spi_init()
{
    DDRB |= _BV(MOSI_PIN) | _BV(SCK_PIN);
    PORTB &= ~_BV(SCK_PIN);
}

uint8_t spi_send(uint8_t c) {
    for (uint8_t i = 0; i < 8; i++)
    {
        if (c & 0x80)
            PORTB |= _BV(MOSI_PIN);
        else
            PORTB &= ~_BV(MOSI_PIN);
	    
        PORTB |= _BV(SCK_PIN);
        c <<= 1;
	    
        if(PINB & _BV(MISO_PIN))
            c |= 1;
	    
        PORTB &= ~_BV(SCK_PIN);
    }
    return c;
}
#endif

void mirf_init() 
{
#ifndef NRF2PIN
    DDRB |= _BV(CSN_PIN);
#endif

    spi_init();
    MIRF_CSN_HI;
}

void mirf_config() 
{
    mirf_config_register(RF_CH, MIRF_CH);
    mirf_config_register(RX_PW_P0, MIRF_PAYLOAD);
}

void mirf_set_TADDR(uint8_t * adr)
{
    mirf_write_register(TX_ADDR, adr,5);
}

void mirf_config_register(uint8_t reg, uint8_t value)
{
    MIRF_CSN_LO;
    spi_send(W_REGISTER | (REGISTER_MASK & reg));
    spi_send(value);
    MIRF_CSN_HI;
}

void mirf_read_register(uint8_t reg, uint8_t * value, uint8_t len)
{
    MIRF_CSN_LO;
    spi_send(R_REGISTER | (REGISTER_MASK & reg));
    for(uint8_t i = 0; i < len; i++) {  // Write payload and read responce
        value[i] = spi_send(value[i]);
    }
    MIRF_CSN_HI;
}

void mirf_write_register(uint8_t reg, uint8_t * value, uint8_t len) 
{
    MIRF_CSN_LO;
    spi_send(W_REGISTER | (REGISTER_MASK & reg));
    for(uint8_t i = 0; i < len; i++) {  // Write payload
        spi_send(value[i]);
    }
    MIRF_CSN_HI;
}

void mirf_send(uint8_t * value, uint8_t len) 
{
    TX_POWERUP;                         // Power up
    MIRF_CSN_LO;                        // Pull down chip select
    spi_send(FLUSH_TX);                 // Write cmd to flush tx fifo
    MIRF_CSN_HI;                        // Pull up chip select
    MIRF_CSN_LO;                        // Pull down chip select
    spi_send(W_TX_PAYLOAD);             // Write cmd to write payload
    for(uint8_t i = 0; i < len; i++) {  // Write payload
        spi_send(value[i]);
    }
    MIRF_CSN_HI;                        // Pull up chip select
}
