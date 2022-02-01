#include <avr/io.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>
#include <stdint.h>
#include <util/delay.h>

#define SENSOR_ID 		1
#define WTD_CYCLE		7

#define DHT_ADDR        0x38
#define SDA_PIN 		PB1
#define SCL_PIN 		PB2
#define AUX_PIN			PB4
#define ADC_PIN			PB3

#include "mirf_tx.h"
#include "nRF24L01.h"

//#define I2C_TIMEOUT 100
//#define I2C_SLOWMODE 1

#include "i2c.h"

char wtdcntr = 0;

uint16_t  adcRead() {
	//Put SDA_PIN to GND for measure vBat (look at schema)
	DDRB |= _BV(SDA_PIN);  //pin as output
    PORTB &= ~_BV(SDA_PIN);

	ADCSRA |= _BV(ADPS2); //1.2M / 16 = 75K
	ADMUX = _BV(REFS0) | ADC_PIN;   	// select internal band gap as reference and chose the ADC channel
	
	ADCSRA |= _BV(ADEN);                // switch on the ADC in general
	
	ADCSRA |= _BV(ADSC);                // start a single ADC conversion
	while( ADCSRA & _BV(ADSC) );        // wait until conversion is complete

	// data sheet recommends to discard first conversion, so we do one more
	ADCSRA |= _BV(ADSC);                // start a single ADC conversion
	while( ADCSRA & _BV(ADSC) );        // wait until conversion is complete
	
	uint16_t adc = ADC;    		// take over ADC reading result into variable

	ADCSRA &= ~(1<<ADEN);               // completely disable the ADC to save power

	return adc;
}

#ifdef DEBUG_UART

#define SOFT_TX_PIN (1 << PB4) // PB1 будет работать как TXD 
#define SOFT_TX_PORT PORTB
#define SOFT_TX_DDR DDRB

void uart_tx_init()
{
  TCCR0A = 1 << WGM01;		// compare mode
  TCCR0B = 1 << CS00;		// prescaler 1
  SOFT_TX_PORT |= SOFT_TX_PIN;
  SOFT_TX_DDR |= SOFT_TX_PIN;
  OCR0A = 78;			//115200 baudrate at prescaler 1
}

void uart_send_byte(unsigned char data)
{
  unsigned char i;
  TCCR0B = 0;
  TCNT0 = 0;
  TIFR0 |= 1 << OCF0A;
  TCCR0B |= (1 << CS00);
  TIFR0 |= 1 << OCF0A;
  SOFT_TX_PORT &= ~SOFT_TX_PIN;
  while (!(TIFR0 & (1 << OCF0A)));
  TIFR0 |= 1 << OCF0A;
  for (i = 0; i < 8; i++)
  {
    if (data & 1)
      SOFT_TX_PORT |= SOFT_TX_PIN;
    else
      SOFT_TX_PORT &= ~SOFT_TX_PIN;
    data >>= 1;
    while (!(TIFR0 & (1 << OCF0A)));
    TIFR0 |= 1 << OCF0A;
  }
  SOFT_TX_PORT |= SOFT_TX_PIN;
  while (!(TIFR0 & (1 << OCF0A)));
  TIFR0 |= 1 << OCF0A;
}

void uart_print(char *str)
{
  byte i = 0;
  while (str[i]) {
    uart_send_byte(str[i++]);
  }
}

void itoa(uint16_t n, char s[]) {
	 uint8_t i = 0;
	 do { s[i++] = n % 10 + '0'; } 
	 while ((n /= 10) > 0);
	 s[i] = '\0';
	 // Reversing
	 uint8_t j;
	 char c;
	 for (i = 0, j = strlen(s)-1; i<j; i++, j--) {
		 c = s[i];
		 s[i] = s[j];
		 s[j] = c;
	 }
 }

#endif

void dataSend() 
{
	uint8_t data[10] = {0};
	
	i2c_init();
    i2c_start(DHT_ADDR << 1);
    i2c_write(0xAC);
    i2c_write(0x33);
    i2c_write(0x00);
    i2c_stop();

    i2c_start((DHT_ADDR << 1)|0x01);
    byte status = i2c_read(true);
    i2c_stop();

    if(status & _BV(7)) {
        _delay_ms(80);
    }
    
    i2c_start((DHT_ADDR<<1)|0x01);
    for (uint8_t cnt=0; cnt < 6; cnt++) 
    {
      data[cnt] = i2c_read(cnt == 5);
    }
    i2c_stop();

	uint16_t adc = adcRead();

	data[6] = adc >> 8;
	data[7] = adc & 0xFF;
	data[8] = SENSOR_ID;
	data[9] = SENSOR_ID;

#ifdef DEBUG_UART
	char buff[8];

	uart_tx_init();
	
	uart_print("ADC: ");
	itoa(adc, buff);
	uart_print(buff);
	uart_print("\r\n");
#endif

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

	// Turn off all: all PB pins as input and internal pullups turned on
	DDRB = 0x00;
	PORTB = _BV(AUX_PIN) | _BV(MOMI_PIN); // turn on pullups on MOMI and AUX
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

	mirf_config_register(RF_CH, MIRF_CH);
	mirf_config_register(RX_PW_P0, MIRF_PAYLOAD);

	mirf_config_register(SETUP_RETR, (1 << 4) | 15); // retransmit 15 times with 2ms delay
	mirf_config_register(EN_AA, 0);
	mirf_config_register(RF_SETUP, (1<<5));  // 250 kbps and 0dBm 

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