#include <avr/wdt.h>
#include "mirf.h"
#include "mirf.c"
#include "spi.h"
#include "spi.c"
#include "nRF24L01.h"
#include "U8glib.h"

#include <Arduino.h>

#define SENSOR_COUNT 32
#define LINE_HEIGHT 8

struct SSensorInfo
{
    int uid;
    float temp;
    float hum;
    float batt;
    float time;
};

uint8_t DSdata[8];
uint8_t rf_setup;
char str_buf[32];

SSensorInfo sensors[SENSOR_COUNT];

U8GLIB_ST7920_128X64 u8g(4, 3, 2, U8G_PIN_NONE);

void setup() {
    Serial.begin(57600);
    delay(2);
    Serial.println("Mirf Init");

    for(int i=0; i<SENSOR_COUNT; i++)
    {
        sensors[i].uid = -1;
        sensors[i].temp = 0.0f;
        sensors[i].hum = 0.0f;
        sensors[i].batt = 0.0f;
        sensors[i].time = 0.0f;
    }

    u8g.setFont(u8g_font_6x10); 
    u8g.setColorIndex(1);
    
    mirf_init();
    mirf_config();

    mirf_config_register(SETUP_RETR, (0 << 4) | 15); // retransmit 15 times with minimal delay
    mirf_config_register(EN_AA, 1); // auto-ack enabled
    //mirf_config_register(RF_SETUP, (1<<5) | (3 << 1)); // 250 kbps and 0dBm 

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
    
    u8g.firstPage();
    do { 
        u8g.setPrintPos(42, 32);
        u8g.print("No data...");
    } while( u8g.nextPage() );  
}

SSensorInfo& getSensor(int _uid)
{
    for(int i=0; i<SENSOR_COUNT; i++)
    {
        if(sensors[i].uid == _uid)
            return sensors[i];
    }
    for(int i=0; i<SENSOR_COUNT; i++)
    {
        if(sensors[i].uid == -1)
        {
            sensors[i].uid = _uid;
            return sensors[i];
        }
    }
    float time = 0.0f;
    int id = 0;
    for(int i=0; i<SENSOR_COUNT; i++)
    {
        if(sensors[i].time > time)
            id = i;
    }

    sensors[id].uid = _uid;
    return sensors[id];
}

void loop() {
    /*for(int i=0; i<SENSOR_COUNT; i++)
    {
        if(sensors[i].uid != -1)
            sensors[i].time += 0.1f;
    }*/

    if(mirf_data_ready())  {
        mirf_read_register( STATUS, &rf_setup, sizeof(rf_setup));
        mirf_get_data(DSdata);

        SSensorInfo& sensor = getSensor(DSdata[7]);

        sensor.time = 0.0f;
        sensor.batt = float(DSdata[6]) / 255.0f;

        uint32_t uit = ((uint32_t)(DSdata[3] & 0x0F) << 16) | ((uint16_t)DSdata[4] << 8) | DSdata[5]; //20-bit raw temperature data
        sensor.temp = (float)uit * 0.000191 - 50;

        uint32_t uih = (((uint32_t)DSdata[1] << 16) | ((uint16_t)DSdata[2] << 8) | (DSdata[3])) >> 4; //20-bit raw humidity data
        sensor.hum = (float)uih * 0.000095;

        Serial.print("ID: ");
        Serial.print(sensor.uid);
        Serial.print(" Vbat.: ");
        Serial.print(DSdata[6]);
        Serial.print(" Humid.: ");
        Serial.print(sensor.hum);
        Serial.print("%, Temp.: ");
        Serial.print(sensor.temp);
        Serial.print("' (CRC: ");
        Serial.print(0, HEX);
        Serial.println(")");

        u8g.firstPage();
        do { 
            int y_pos = 7;
            for(int i=0; i<SENSOR_COUNT; i++)
            {
                if(sensors[i].uid != -1)
                {
                    sprintf(str_buf, "#%02d t%02.2f' h%02.2f%% (%.2f%%)", sensors[i].uid, sensors[i].temp, sensors[i].hum, sensors[i].batt);
                    u8g.setPrintPos(0, y_pos);
                    u8g.print("#");
                    u8g.print(sensors[i].uid, 1);
                    u8g.print(" t");
                    u8g.print(sensors[i].temp, 1);
                    u8g.print("' h");
                    u8g.print(sensors[i].hum, 1);
                    u8g.print("% b");
                    u8g.print(int(sensors[i].batt * 100));
                    u8g.print("%");

                    y_pos += LINE_HEIGHT;
                }
            }
        } while( u8g.nextPage() );
    }
}
