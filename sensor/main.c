#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>
#include <stdint.h>
#include <util/delay.h>

#include "mirf_tx.h"
#include "nRF24L01.h"

#define SENSOR_ID 		3

#define DHT_BIT         PB1
#define ADC_BIT			PB3

// watchdog interrupt
ISR(WDT_vect) {
	WDTCR |= _BV(WDTIE);  // разрешаем прерывания по ватчдогу. Иначе будет резет.
}

void sleepFor8Secs(int oct) {
	cli();
	MCUSR = 0;   // clear various "reset" flags
	//инициализация ватчдога
	wdt_reset();  // сбрасываем
	if(oct == 1) {
		wdt_enable(WDTO_1S);  // разрешаем ватчдог 8 сек
	}else {
		wdt_enable(WDTO_500MS);  // разрешаем ватчдог 1 сек
	}
	//WDTCR |= _BV(WDCE);
	WDTCR &= ~_BV(WDE);
	WDTCR |= _BV(WDTIE);  // разрешаем прерывания по ватчдогу. Иначе будет резет.
	sei();  // разрешаем прерывания
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
	sleep_enable();
	sleep_cpu();
	sleep_disable();  // cancel sleep as a precaution
}

inline uint8_t adcRead() {
	DDRB |= _BV(DHT_BIT);  //pin as output
    PORTB &= ~_BV(DHT_BIT);

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

inline int dht22read() {
	// если в программе имеются прерывания,то не забывайте их отлючать перед чтением датчика
	uint8_t  j = 0, i = 0;
	uint8_t datadht[8];
	
	_delay_ms(2000);  //sleep till the intermediate processes in the accessories are settled down

    DDRB |= _BV(DHT_BIT);  //pin as output
    PORTB &= ~_BV(DHT_BIT);
	_delay_ms(18);
	PORTB |= _BV(DHT_BIT);
	DDRB &= ~_BV(DHT_BIT);
	_delay_us(50);  // +1 для attiny(коррекция без кварца)
	if(PINB & _BV(DHT_BIT)) return 3;
	_delay_us(80);  // +1 для attiny(коррекция без кварца)
	if(!(PINB & _BV(DHT_BIT))) return 4;
	while (PINB & _BV(DHT_BIT)) ;
	for (j = 0; j < 5; j++) {
		datadht[j] = 0;
		for (i = 0; i < 8; i++) {
			while (!(PINB & _BV(DHT_BIT))) ;
			_delay_us(30);
			if (PINB & _BV(DHT_BIT)) 
				datadht[j] |= 1 << (7 - i);
			while (PINB & _BV(DHT_BIT)) ;
		}
	}
	if (datadht[4] == ((datadht[0] + datadht[1] + datadht[2] + datadht[3]) & 0xFF)) {

		datadht[5] = adcRead();
		datadht[6] = SENSOR_ID;

		mirf_init();

        MIRF_CSN_LO;
		_delay_ms(100);
		MIRF_CSN_HI;

		// need time to come out of power down mode s. 6.1.7, table 16  datasheet says 1.5ms max, tested as low as 600us
		mirf_config_register(CONFIG, MIRF_CONFIG | (1 << PWR_UP));
		_delay_ms(1);

		mirf_send(datadht, MIRF_PAYLOAD);
		_delay_us(10);
		mirf_config_register(CONFIG, MIRF_CONFIG);
		sleepFor8Secs(0);
		// Reset status register for further interaction 
		mirf_config_register(STATUS, (1 << TX_DS));  // Reset status register
	
		return 0;
	}
	return 1;
}

int main()
{	
	int f;

	DDRB = 0; //Turn off PORTB
	PORTB = 0; //Turn off PORTB
	ADCSRA = 0;        //turn off ADC

    mirf_init();

	// wait for mirf - is this necessary?
	_delay_ms(50);

	mirf_config();

	mirf_config_register(SETUP_RETR, 0);  // no retransmit 
	// disable enhanced shockburst
	mirf_config_register(EN_AA, 0);  // no auto-ack  
	mirf_set_TADDR((uint8_t *)"2Sens");
    	
	for (;;) {
		f = dht22read();
		if (f == 0) {
			DDRB = 0; //Turn off PORTB
			PORTB = 0; //Turn off PORTB

			ADCSRA = 0;        //turn off ADC

			sleepFor8Secs(1);
		}
	}
}
