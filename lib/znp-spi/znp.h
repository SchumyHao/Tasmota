#ifndef znp_h
#define znp_h

#define TYPE_COORDINATOR 0x00
#define TYPE_ROUTER 0x01
#define TYPE_ENDDEVICE 0x02

#define ZCD_NV_LOGICAL_TYPE 0x0087
#define ZCD_NV_PAN_ID 0x0083
#define ZCD_NV_CHANLIST 0x0084

#include <Arduino.h>
#include <SPI.h>

//AREQ - Asynchronous Request
//SREQ - Synchronous request
//POLL - Poll request

#define MAX_DATA_FRAME_LEN  256

class ZNPDataFrame {
  public:
    ZNPDataFrame(uint8_t len, uint8_t c0, uint8_t c1, uint8_t *m);
    ZNPDataFrame(uint8_t *m);
    uint8_t get_length() const;
    uint8_t get_cmd0() const;
    uint8_t get_cmd1() const;
    uint8_t* get_msg() const;
    void set_length(uint8_t d);
    void set_cmd0(uint8_t d);
    void set_cmd1(uint8_t d);
  private:
    uint8_t length;
    uint8_t cmd0;
    uint8_t cmd1;
    uint8_t *msg;
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
    int poll(ZNPDataFrame *rdf);
    int sreq(const ZNPDataFrame *sdf, ZNPDataFrame *rdf);
    void areq(const ZNPDataFrame *sdf);
    bool available();
  private:
    int SPI_MODE;
    int PIN_MOSI;
    int PIN_MISO;
    int PIN_SCK;
    int PIN_CS;
    int PIN_SS_MRDY;
    int PIN_SRDY;
    int read_response(ZNPDataFrame *rdf);
    bool wait_srdy_timeout(int target_level, int ms);
};

#endif
