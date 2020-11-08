#include <avr/wdt.h>
#include "mirf.h"
#include "mirf.c"
#include "spi.h"
#include "spi.c"
#include "nRF24L01.h"

#define USE_MICRO_WIRE

#include <OLED.h>

uint8_t DSdata[16];
uint8_t rf_setup;
int recieved = 0;
int wait = 0;

GyverOLED oled;

void setup() {
    Serial.begin(57600);
    delay(2);
    Serial.println("Mirf Init");

    //Need for delay 50ms before init dispaly
    delay(50);
    oled.init(OLED128x32);

    oled.inverse(false);
    oled.clear();
    
    mirf_init();
    mirf_config();
    //mirf_config_register(SETUP_RETR, 0 ); // no retransmit 
    mirf_config_register(EN_AA, 0 ); // no auto-ack 
    //mirf_config_register(RF_SETUP, (1<<RF_DR_LOW) ); // low spd & power 
    mirf_set_TADDR((uint8_t *)"1Serv");
    mirf_set_RADDR((uint8_t *)"1Sens");
    mirf_config_register(RX_ADDR_P2,0x32);
    mirf_config_register(RX_PW_P2, mirf_PAYLOAD);
    mirf_config_register(RX_ADDR_P3,0x33);
    mirf_config_register(RX_PW_P3, mirf_PAYLOAD);
    mirf_config_register(RX_ADDR_P4,0x34);
    mirf_config_register(RX_PW_P4, mirf_PAYLOAD);
    mirf_config_register(RX_ADDR_P5,0x35);
    mirf_config_register(RX_PW_P5, mirf_PAYLOAD);
    mirf_config_register(EN_RXADDR, 0x7F);

    Serial.println("Setup OK");
    oled.setCursor(0, 0);
    oled.scale2X();
    oled.print("Wait...");
}

void loop() {
    //wdt_reset();
    delay(500);
    wait++;

    char str_buf[32];

    if(mirf_data_ready())  {
        mirf_read_register( STATUS, &rf_setup, sizeof(rf_setup));
        mirf_get_data(DSdata);

        recieved++;
        wait=0;

        int iH = (DSdata[0] << 8) | DSdata[1];
        float fH = iH / 10.0f;

        int iT = ((DSdata[2] & 0x7f ) << 8 ) + DSdata[3];
        if (DSdata[2] & 0x80)
            iT *= -1;
            
        float fT = iT / 10.0f;

        sprintf(str_buf, "%02d' h%02d%%", (int)(fT > 0 ? fT : -fT), (int)fH);

        oled.setCursor(0, 0);
        oled.scale2X();
        oled.print(fT > 0 ? "t+" : "t-");
        oled.print(str_buf);

        Serial.print("ID: ");
        Serial.print(DSdata[6]);
        Serial.print(" Vbat.: ");
        Serial.print(DSdata[5]);
        Serial.print(" Humid.: ");
        Serial.print(fH);
        Serial.print("%, Temp.: ");
        Serial.print(fT);
        Serial.print("' (CRC: ");
        Serial.print(DSdata[4], HEX);
        Serial.println(")");
    }

    //sprintf(str_buf,"recv: %03d wait: %03d", recieved%1000, (wait/2)%1000);
    //int pcnt = (DSdata[5] * 99) / 255;
    //sprintf(str_buf,"I:%01d B:%02d T:%03d C:%03d", DSdata[6], (uint8_t)pcnt, (wait/2)%1000, recieved%1000);
    sprintf(str_buf,"5:%03d 6:%03d  ", DSdata[5], DSdata[6]);

    oled.setCursor(0, 3);
    oled.scale1X();
    oled.print(str_buf);
}
