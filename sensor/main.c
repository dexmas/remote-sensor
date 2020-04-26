#include <avr/io.h>
#include <avr/wdt.h> // ����� ������������ ������ � ���������
#include <avr/sleep.h> // ����� ������� ������ ���
#include <avr/interrupt.h> // ������ � ������������
#include <stdint.h>
#include <util/delay.h>

#include "mirf_tx.h"
#include "nRF24L01.h"

#define DHT_BIT         3 // pin 3 -  TRANSMITPIN
#define DHT_PORT        PORTB // ����
#define DHT_DDR         DDRB
#define DHT_PIN         PINB

#define SLEEPDURATION 1//0 //sleep duration in seconds/8, shall be a factor of 8

// watchdog interrupt
ISR(WDT_vect) {
	WDTCR |= _BV(WDTIE);  // ��������� ���������� �� ��������. ����� ����� �����.
}

void sleepFor8Secs(int oct) {
	cli();
	MCUSR = 0;   // clear various "reset" flags
	//������������� ��������
	wdt_reset();  // ����������
	if(oct == 1) {
		wdt_enable(WDTO_8S);  // ��������� ������� 8 ���
	}else {
		wdt_enable(WDTO_1S);  // ��������� ������� 1 ���
	}
	//WDTCR |= _BV(WDCE);
	WDTCR &= ~_BV(WDE);
	WDTCR |= _BV(WDTIE);  // ��������� ���������� �� ��������. ����� ����� �����.
	sei();  // ��������� ����������
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
	sleep_enable();
	sleep_cpu();
	sleep_disable();  // cancel sleep as a precaution
}

int dht22read() {
	// ���� � ��������� ������� ����������,�� �� ��������� �� �������� ����� ������� �������
	uint8_t  j = 0, i = 0;
	uint8_t datadht[8];
	
	_delay_ms(2000);  //sleep till the intermediate processes in the accessories are settled down

    DHT_DDR |= (1 << DHT_BIT);  //pin as output
    DHT_PORT &= ~(1 << DHT_BIT);
	_delay_ms(18);
	DHT_PORT |= (1 << DHT_BIT);
	DHT_DDR &= ~(1 << DHT_BIT);
	_delay_us(50);  // +1 ��� attiny(��������� ��� ������)
	if(DHT_PIN&(1 << DHT_BIT)) return 3;
	_delay_us(80);  // +1 ��� attiny(��������� ��� ������)
	if(!(DHT_PIN&(1 << DHT_BIT))) return 4;
	while (DHT_PIN&(1 << DHT_BIT)) ;
	for (j = 0; j < 5; j++) {
		datadht[j] = 0;
		for (i = 0; i < 8; i++) {
			while (!(DHT_PIN&(1 << DHT_BIT))) ;
			_delay_us(30);
			if (DHT_PIN&(1 << DHT_BIT)) 
				datadht[j] |= 1 << (7 - i);
			while (DHT_PIN&(1 << DHT_BIT)) ;
		}
	}
	if (datadht[4] == ((datadht[0] + datadht[1] + datadht[2] + datadht[3]) & 0xFF)) {
        mirf_CSN_lo;
		_delay_ms(100);
		mirf_CSN_hi;

		// need time to come out of power down mode s. 6.1.7, table 16  datasheet says 1.5ms max, tested as low as 600us
		mirf_config_register(CONFIG, mirf_CONFIG | (1 << PWR_UP));
		_delay_ms(1);

		mirf_send(datadht, mirf_PAYLOAD);
		_delay_us(10);
		mirf_config_register(CONFIG, mirf_CONFIG);
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
	//PORTB &= 0;  // turn off the pin power
	//DDRB &= 0;  //change pin mode to reduce power consumption
	DDRB |= (1 << 4);

	ADCSRA &= ~(1 << ADEN);                      //turn off ADC
	ACSR |= _BV(ACD);                          //disable the analog comparator

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
			for (f = 0; f < SLEEPDURATION; f++) {
				sleepFor8Secs(1);
			}
		}
	}
}
