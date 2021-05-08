#include <avr/io.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>
#include <stdint.h>
#include <util/delay.h>

#include "mirf_tx.h"
#include "nRF24L01.h"

#define SENSOR_ID 		1
#define ADC_BIT			PB3
#define DHT_SDA			PB1
#define DHT_SCL			SCK_PIN
#define WTD_CYCLE		7

#define DHT_ADDR        0x38
#define SDA_PIN DHT_SDA
#define SCL_PIN DHT_SCL

//#define I2C_TIMEOUT 100
//#define I2C_SLOWMODE 1

#include "i2c.h"

char wtdcntr = 0;

inline uint8_t adcRead() {
	//Put DHT_SDA to GND for measure vBat (look at schema)
	DDRB |= _BV(DHT_SDA);  //pin as output
    PORTB &= ~_BV(DHT_SDA);

	ADCSRA |= _BV(ADPS2); // ADC prescaler :128, that gives ADC frequency of 1.2MHz/16 = 75kHz
	ADMUX = _BV(REFS0) | ADC_BIT;   	// select internal band gap as reference and chose the ADC channel
	
	ADCSRA |= _BV(ADEN);                // switch on the ADC in general
	
	ADCSRA |= _BV(ADSC);                // start a single ADC conversion
	while( ADCSRA & _BV(ADSC) );        // wait until conversion is complete

	// data sheet recommends to discard first conversion, so we do one more
	ADCSRA |= _BV(ADSC);                // start a single ADC conversion
	while( ADCSRA & _BV(ADSC) );        // wait until conversion is complete
	
	uint8_t value = ADCL;        		// take over ADC reading result into variable
	if(ADCH == 0x01)
		value = 0x00;
	if(ADCH == 0x03)
		value = 0xFF;

	ADCSRA &= ~(1<<ADEN);               // completely disable the ADC to save power

	return value;
}

void dataSend() {
	uint8_t data[8] = {0};
	
	i2c_init();
    i2c_start(DHT_ADDR<<1);
    i2c_write(0xAC);
    i2c_write(0x33);
    i2c_write(0x00);
    i2c_stop();

    i2c_start((DHT_ADDR<<1)|0x01);
    byte status = i2c_read(true);
    i2c_stop();

    if(!(status & _BV(3))) {
        return 1;
    }

    if(status & _BV(7)) {
        _delay_ms(80);
    }
    
    i2c_start((DHT_ADDR<<1)|0x01);
    for (uint8_t cnt=0; cnt < 6; cnt++) 
    {
      data[cnt] = i2c_read(cnt == 5);
    }
    i2c_stop();

	data[6] = adcRead();
	data[7] = SENSOR_ID;

	mirf_init();

	MIRF_CSN_LO;
	_delay_ms(100);
	MIRF_CSN_HI;

	// need time to come out of power down mode s. 6.1.7, table 16  datasheet says 1.5ms max, tested as low as 600us
	mirf_config_register(CONFIG, MIRF_CONFIG | (1 << PWR_UP));
	_delay_ms(1);

	mirf_send(data, MIRF_PAYLOAD);
	_delay_us(10);

	mirf_config_register(STATUS, (1 << TX_DS));  // Reset status register
	_delay_us(2);

	mirf_config_register(CONFIG, MIRF_CONFIG); //Power down

	DDRB = _BV(DHT_SDA) | _BV(SCK_PIN) | _BV(MOMI_PIN);
	PORTB = _BV(DHT_SDA) | _BV(SCK_PIN); // turn off all
}

// watchdog interrupt
ISR(WDT_vect) {
	wtdcntr++;
	if(wtdcntr >= WTD_CYCLE)
	{
		cli();
		dataSend();
		wtdcntr = 0;
		sei();
	}
}

int main()
{
	cli();

	//Init AHT10
	i2c_init();
	_delay_ms(25);

    i2c_start(DHT_ADDR<<1);
    i2c_write(0xA8);
    i2c_write(0x00);
    i2c_write(0x00);
    i2c_stop();

    _delay_ms(100);

    i2c_start(DHT_ADDR<<1);
    i2c_write(0xE1);
    i2c_write(0x08);
    i2c_write(0x00);
    i2c_stop();

    _delay_ms(100);

    //i2c_start((DHT_ADDR<<1)|0x01);
    //byte status = i2c_read(true);
    //i2c_stop();

    mirf_init();

	mirf_config();

	mirf_config_register(SETUP_RETR, (1 << 4) | 15); // retransmit 15 times with 500ns delay
	mirf_config_register(EN_AA, 0);  // no auto-ack  
	//mirf_config_register(RF_SETUP, (1<<5) | (3 << 1));  // 250 kbps and 0dBm 

	mirf_set_TADDR((uint8_t *)"2Sens");

	dataSend();
    	
	// prescale timer to 8s so we can measure current
	WDTCR |= (1<<WDP3 )|(0<<WDP2 )|(0<<WDP1)|(1<<WDP0); // 8s
	// Enable watchdog timer interrupts
	WDTCR |= (1<<WDTIE);
	sei(); // Enable global interrupts
	// Use the Power Down sleep mode
	set_sleep_mode(SLEEP_MODE_PWR_DOWN);
	while(1) {
		sleep_mode();   // go to sleep and wait for interrupt...
	}
}