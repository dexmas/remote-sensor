#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>
#include <stdint.h>
#include <util/delay.h>

#include "mirf_tx.h"
#include "nRF24L01.h"

#define SENSOR_ID 		3
#define ADC_BIT			PB3
#define DHT_SDA			PB1
#define DHT_SCL			PB4

#define DHT_ADDR        0x38
#define SDA_PORT PORTB
#define SCL_PORT PORTB
#define SDA_PIN DHT_SDA
#define SCL_PIN DHT_SCL

//#define I2C_TIMEOUT 100
//#define I2C_SLOWMODE 1

#include "i2c.h"

// watchdog interrupt
ISR(WDT_vect) {
	WDTCR |= _BV(WDTIE);  // разрешаем прерывания по ватчдогу. Иначе будет резет.
}

void sleepFor8Secs() {
	cli();
	MCUSR = 0;   // clear various "reset" flags
	//инициализация ватчдога
	wdt_reset();  // сбрасываем
	wdt_enable(WDTO_8S);  // разрешаем ватчдог 8 сек

	//WDTCR |= _BV(WDCE);
	WDTCR &= ~_BV(WDE);
	WDTCR |= _BV(WDTIE);  // разрешаем прерывания по ватчдогу. Иначе будет резет.
	sei();  // разрешаем прерывания
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
	sleep_enable();
	sleep_cpu();
}

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
	
	uint8_t value = ADC;               // take over ADC reading result into variable
	
	ADCSRA = 0;                         // completely disable the ADC to save power

	return value;
}

int dataRead() {
	uint8_t data[8] = {0};
	
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
	mirf_config_register(CONFIG, MIRF_CONFIG);
	_delay_us(20);

	// Reset status register for further interaction 
	mirf_config_register(STATUS, (1 << TX_DS));  // Reset status register

	return 0;
}

int main()
{	
    mirf_init();

	// wait for mirf - is this necessary?
	_delay_ms(50);

	mirf_config();

	mirf_config_register(SETUP_RETR, 0);  // no retransmit 
	// disable enhanced shockburst
	mirf_config_register(EN_AA, 0);  // no auto-ack  
	mirf_set_TADDR((uint8_t *)"2Sens");

	//Init AHT10
	i2c_init();
	_delay_ms(40);

    i2c_start(DHT_ADDR<<1);
    i2c_write(0xA8);
    i2c_write(0x00);
    i2c_write(0x00);
    i2c_stop();

    _delay_ms(350);

    i2c_start(DHT_ADDR<<1);
    i2c_write(0xE1);
    i2c_write(0x08);
    i2c_write(0x00);
    i2c_stop();

    _delay_ms(350);

    i2c_start((DHT_ADDR<<1)|0x01);
    byte status = i2c_read(true);
    i2c_stop();
    	
	while(1) {
		if (!dataRead()) {
			PORTB = 0;  //Turn off PORTB
			ACSR |= _BV(ACD); //turn off comparator

			sleepFor8Secs();
		}
	}
}
