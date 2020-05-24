#ifndef znp_h
#define znp_h

//#define USE_UART 1
#define USE_SPI 1

#define PIN_RX      12
#define PIN_TX      11
#define PIN_CTS      3
#define PIN_RTS      2

#define TYPE_COORDINATOR 0x00
#define TYPE_ROUTER 0x01
#define TYPE_ENDDEVICE 0x02

#define ZCD_NV_LOGICAL_TYPE 0x0087
#define ZCD_NV_PAN_ID 0x0083
#define ZCD_NV_CHANLIST 0x0084

#include <Arduino.h>

#ifdef USE_SPI
#include <SPI.h>
#endif

#ifdef USE_UART
#include <SoftwareSerial.h>
#include <stdint.h> 
#endif

//AREQ - Asynchronous Request
//SREQ - Synchronous request
//POLL - Poll request

#define MAX_DATA_FRAME_LEN  256

class ZNPDataFrame {
  public:
    uint8_t length;
    uint8_t cmd0;
    uint8_t cmd1;
    uint8_t *msg;
    ZNPDataFrame(int max_len) {
      length = 0;
      cmd0 = 0;
      cmd1 = 0;
      msg = new uint8_t[max_len];
      _need_free = true;
    }
    ZNPDataFrame() {
      ZNPDataFrame(MAX_DATA_FRAME_LEN);
    }
    ZNPDataFrame(uint8_t len, uint8_t c0, uint8_t c1, uint8_t *m) {
      length = len;
      cmd0 = c0;
      cmd1 = c1;
      msg = m;
      _need_free = false;
    }
    ~ZNPDataFrame() {
      if (_need_free)
        delete[] msg;
    }
  private:
    bool _need_free;
};

class ZNP {
  public:
    ZNP(int mosi, int miso, int sck, int cs, int mrdy, int srdy, int mode) {
      PIN_MOSI = mosi;
      PIN_MISO = miso;
      PIN_SCK = sck;
      PIN_CS = cs;
      PIN_SS_MRDY = mrdy;
      PIN_SRDY = srdy;
      SPI_MODE = mode;
    }
    void init();
    void poll(ZNPDataFrame &rdf);
    void sreq(const ZNPDataFrame &sdf, ZNPDataFrame &rdf);
    void areq(const ZNPDataFrame &sdf);
    //poll_response set_config(uint8_t config_id, uint8_t len, uint8_t * msg);
    //poll_response set_logical_type(uint8_t t);
    //poll_response set_pan_id(uint16_t pan_id);
    //poll_response set_chanlist(uint32_t chanlist);
    //poll_response app_register(uint8_t app_end_point, uint16_t app_profile_id);
    //poll_response start_request();
    bool available();
#ifdef USE_UART
    SoftwareSerial *serial;
#endif
  private:
    int SPI_MODE;
    int PIN_MOSI;
    int PIN_MISO;
    int PIN_SCK;
    int PIN_CS;
    int PIN_SS_MRDY;
    int PIN_SRDY;
    void read_response(ZNPDataFrame &rdf);
};

#endif
