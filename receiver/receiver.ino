#include <avr/wdt.h>
#include "mirf.h"
#include "nRF24L01.h"

#include <Arduino.h>

#include <SPI.h>
#include "Ucglib.h"

#define SENSOR_COUNT 32

#define TFT_DC  4
#define TFT_RST 3
#define TFT_CS  2
#define TFT_LED 5

struct SSensorInfo
{
    uint8_t uid;
    float temp;
    float hum;
    float batt;
    float time;
};

uint8_t DSdata[8];
uint8_t rf_setup;
char str_buf[64];

Ucglib_ILI9341_18x240x320_HWSPI ucg(TFT_DC, TFT_CS, TFT_RST);

SSensorInfo sensors[SENSOR_COUNT];

void setup() {
    Serial.begin(9600);
    delay(2);
    Serial.println("Mirf Init");

    for(int i=0; i<SENSOR_COUNT; i++)
    {
        sensors[i].uid = 255;
        sensors[i].temp = 0.0f;
        sensors[i].hum = 0.0f;
        sensors[i].batt = 0.0f;
        sensors[i].time = 0.0f;
    }
    
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

    // Enable LCD LED
    pinMode(TFT_LED, OUTPUT);
    digitalWrite(TFT_LED, HIGH);

    ucg.begin(UCG_FONT_MODE_TRANSPARENT);
    ucg.setRotate270();

    ucg.setColor(0, 0, 0, 60);
    ucg.drawBox(0, 0, 320, 240);
    ucg.setColor(0, 255, 0, 0);
    ucg.setFontMode(UCG_FONT_MODE_TRANSPARENT);
    ucg.setPrintPos(85, 130);
    ucg.setFont(ucg_font_logisoso28_hr);
    ucg.print("No data...");

    Serial.println("Setup OK"); 
}

uint8_t getSensor(uint8_t _uid, SSensorInfo& _sensor_out)
{
    //Serial.print("Get sensor for ID: ");
    //Serial.print(_uid);
    uint8_t i = 0;
    for(i = 0; i < SENSOR_COUNT; i++)
    {
        if(sensors[i].uid == _uid)
        {
            //Serial.print(". Fount in place: ");
            //Serial.println(i);
            _sensor_out = sensors[i];
            return i;
        }
    }
    for(i = 0; i < SENSOR_COUNT; i++)
    {
        if(sensors[i].uid == 255)
        {
            //Serial.print(". Not found. Create in place: ");
            //Serial.println(i);
            sensors[i].uid = _uid;
            _sensor_out = sensors[i];
            return i;
        }
    }

    return 255;
}

void DrawSensor(const SSensorInfo& _info, uint8_t _place)
{
    int cellX = _place % 3;
    int cellY = _place / 3;
    int startX = 5 + cellX * 105;
    int startY = 5 + cellY * 80;

    /*Serial.print("Draw sensor ID: ");
    Serial.print(_info.uid);
    Serial.print(", Place: ");
    Serial.print(_place);
    Serial.print(", CellX: ");
    Serial.print(cellX);
    Serial.print(", CellY: ");
    Serial.println(cellY);*/

    if(_info.batt < 0.3)
    {
        ucg.setColor(0, 225, 0, 0);	
        ucg.setColor(1, 225, 0, 0);
        ucg.setColor(2, 225, 0, 0);
        ucg.setColor(3, 225, 0, 0);
    }
    else
    {
        ucg.setColor(0, 0, 0, 255);	
        ucg.setColor(1, 0, 0, 255);
        ucg.setColor(2, 0, 0, 255);
        ucg.setColor(3, 0, 0, 255);
    }

    ucg.drawRBox(startX, startY, 100, 75, 15);

    ucg.setFontMode(UCG_FONT_MODE_TRANSPARENT);
    ucg.setFont(ucg_font_logisoso16_hr);
    ucg.setColor(0, 255, 255, 255);		// use white as main color for the font
    ucg.setPrintPos(startX + 10, startY + 30);
    ucg.print(_info.uid, 1);

    ucg.setColor(0, 255, 255, 0);		// use yellow for temperature
    ucg.setPrintPos(startX + 30, startY + 25);
    ucg.print("t");
    ucg.print(_info.temp, 2);
    ucg.print("'");

    ucg.setColor(0, 0, 255, 0);		// use green for temperature
    ucg.setPrintPos(startX + 30, startY + 45);
    ucg.print("h");
    ucg.print(_info.hum, 1);
    ucg.print("%");

    ucg.setFont(ucg_font_7x14_mf);
    ucg.setColor(0, 180, 180, 180);		// use grey for batt
    ucg.setPrintPos(startX + 20, startY + 65);
    ucg.print("bat. ");
    ucg.print(int(_info.batt * 100));
    ucg.print("%");
}

void loop() {
    if(mirf_data_ready())  {
        mirf_read_register( STATUS, &rf_setup, sizeof(rf_setup));
        mirf_get_data(DSdata);

        if(sensors[0].uid == 255) // First datat receive
        {
          //Clear screen
          ucg.setColor(0, 0, 0, 60);
          ucg.drawBox(0, 0, 320, 240);
        }

        SSensorInfo sensor;
        uint8_t place = getSensor(DSdata[7], sensor);

        if(place == 255) 
        {
            Serial.print("Error. Accordinf place for sensor: ");
            Serial.print(DSdata[7]);
            Serial.println(" Not found. Skip drawing.");

            return;
        }

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

        DrawSensor(sensor, place);
    }
}