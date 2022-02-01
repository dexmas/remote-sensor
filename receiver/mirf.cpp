/*
    Copyright (c) 2007 Stefan Engelke <mbox@stefanengelke.de>

    Permission is hereby granted, free of charge, to any person 
    obtaining a copy of this software and associated documentation 
    files (the "Software"), to deal in the Software without 
    restriction, including without limitation the rights to use, copy, 
    modify, merge, publish, distribute, sublicense, and/or sell copies 
    of the Software, and to permit persons to whom the Software is 
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be 
    included in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, 
    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF 
    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND 
    NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT 
    HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, 
    WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
    DEALINGS IN THE SOFTWARE.

    $Id$
*/

#include "mirf.h"
#include "nrf24l01.h"

#include <SPI.h>

#include <avr/io.h>
#include <avr/interrupt.h>

// Defines for setting the MiRF registers for transmitting or receiving mode
#define TX_POWERUP mirf_config_register(CONFIG, MIRF_CONFIG | ((1<<PWR_UP) | (0 << PRIM_RX)))
#define RX_POWERUP mirf_config_register(CONFIG, MIRF_CONFIG | ((1<<PWR_UP) | (1 << PRIM_RX)))

// Flag which denotes transmitting mode
//volatile uint8_t PTX;

void mirf_init() 
// Initializes pins ans interrupt to communicate with the MiRF module
// Should be called in the early initializing phase at startup.
{
    // Define CSN and CE as Output and set them to default
    DDRD |= ((1<<CSN)|(1<<CE));
    mirf_CE_lo;
    mirf_CSN_hi;

#if defined(__AVR_ATmega8__)
    // Initialize external interrupt 0 (PD2)
    MCUCR = ((1<<ISC11)|(0<<ISC10)|(1<<ISC01)|(0<<ISC00)); // Set external interupt on falling edge
    GICR  = ((0<<INT1)|(1<<INT0));                         // Activate INT0
#endif // __AVR_ATmega8__

#if defined(__AVR_ATmega168__)
    // Initialize external interrupt on port PD6 (PCINT22)
    DDRB &= ~(1<<PD6);
    PCMSK2 = (1<<PCINT22);
    PCICR  = (1<<PCIE2);
#endif // __AVR_ATmega168__    

    // Initialize spi module
    SPI.begin();
    SPI.beginTransaction(SPISettings(10000000UL, MSBFIRST, SPI_MODE0));
}

void mirf_config() 
// Sets the important registers in the MiRF module and powers the module
// in receiving mode
{
    // Set RF channel
    mirf_config_register(RF_CH, MIRF_CH);

    // Set length of incoming payload 
    mirf_config_register(RX_PW_P0, MIRF_PAYLOAD);
    mirf_config_register(RX_PW_P1, MIRF_PAYLOAD);

    // Start receiver 
    //PTX = 0;        // Start in receiving mode
    RX_POWERUP;     // Power up in receiving mode
    mirf_CE_hi;     // Listening for pakets
}

void mirf_set_RADDR(uint8_t * adr) 
// Sets the receiving address
{
    mirf_CE_lo;
    mirf_write_register(RX_ADDR_P1,adr,5);
    mirf_CE_hi;
}

void mirf_set_TADDR(uint8_t * adr)
// Sets the transmitting address
{
    mirf_CE_lo;
    mirf_write_register(RX_ADDR_P0,adr,5);
    mirf_write_register(TX_ADDR, adr,5);
    mirf_CE_hi;    
}

uint8_t mirf_data_ready() 
// Checks if data is available for reading
{
    //if (PTX) return 0;
    uint8_t status;
    // Read MiRF status 
    mirf_CSN_lo;                                // Pull down chip select
    status = SPI.transfer(NOP);               // Read status register
    mirf_CSN_hi;                                // Pull up chip select
    return status & (1<<RX_DR);
}

void mirf_get_data(uint8_t * data) 
{
    mirf_CSN_lo;                               // Pull down chip select
    SPI.transfer(R_RX_PAYLOAD);            // Send cmd to read rx payload
    for(char i = 0; i < MIRF_PAYLOAD; i++)   // Read payload
        data[i] = SPI.transfer(data[i]);
    mirf_CSN_hi;                               // Pull up chip select
    mirf_config_register(STATUS, (1<<RX_DR));   // Reset status register
}

void mirf_config_register(uint8_t reg, uint8_t value)
// Clocks only one byte into the given MiRF register
{
    mirf_CSN_lo;
    SPI.transfer(W_REGISTER | (REGISTER_MASK & reg));
    SPI.transfer(value);
    mirf_CSN_hi;
}

void mirf_read_register(uint8_t reg, uint8_t * value, uint8_t len)
// Reads an array of bytes from the given start position in the MiRF registers.
{
    mirf_CSN_lo;
    SPI.transfer(R_REGISTER | (REGISTER_MASK & reg));
    for(char i = 0; i < len; i++)
        value[i] = SPI.transfer(value[i]);
    mirf_CSN_hi;
}

void mirf_write_register(uint8_t reg, uint8_t * value, uint8_t len) 
// Writes an array of bytes into inte the MiRF registers.
{
    mirf_CSN_lo;
    SPI.transfer(W_REGISTER | (REGISTER_MASK & reg));
    SPI.transfer(value, len);
    mirf_CSN_hi;
}

void mirf_send(uint8_t * value, uint8_t len) 
// Sends a data package to the default address. Be sure to send the correct
// amount of bytes as configured as payload on the receiver.
{
//    while (PTX) {}                  // Wait until last paket is send

    mirf_CE_lo;

//    PTX = 1;                        // Set to transmitter mode
    TX_POWERUP;                     // Power up
    
    mirf_CSN_lo;                    // Pull down chip select
    SPI.transfer(FLUSH_TX);     // Write cmd to flush tx fifo
    mirf_CSN_hi;                    // Pull up chip select
    
    mirf_CSN_lo;                    // Pull down chip select
    SPI.transfer(W_TX_PAYLOAD); // Write cmd to write payload
    SPI.transfer(value, len);   // Write payload
    mirf_CSN_hi;                    // Pull up chip select
    
    mirf_CE_hi;                     // Start transmission
}
