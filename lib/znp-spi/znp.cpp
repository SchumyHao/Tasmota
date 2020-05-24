#include "znp.h"

void ZNP::init()
{
  #ifdef USE_SPI
  pinMode(PIN_SRDY, INPUT);
  pinMode(PIN_SS_MRDY, OUTPUT);
  pinMode(PIN_CS, OUTPUT);
  SPI.setBitOrder(MSBFIRST);
  if (SPI_MODE == 0)
    SPI.setDataMode(SPI_MODE0);
  else
    SPI.setDataMode(SPI_MODE3);
  SPI.setClockDivider(SPI_CLOCK_DIV8);
  #endif

  #ifdef USE_UART
  this->serial = new SoftwareSerial(PIN_RX, PIN_TX);
  this->serial->begin(115200);

  pinMode(PIN_RTS, OUTPUT);
  pinMode(PIN_CTS, INPUT);
  digitalWrite(PIN_RTS, LOW);
  #endif

  #ifdef USE_SPI
  digitalWrite(PIN_SS_MRDY, HIGH);
  digitalWrite(PIN_CS, HIGH);
  SPI.begin();
  #endif
}

bool ZNP::available()
{
  // SRDY low means ZNP has data want to send to AP.
  return !digitalRead(PIN_SRDY);
}

void ZNP::read_response(ZNPDataFrame &rdf)
{
  #ifdef USE_SPI
  digitalWrite(PIN_CS, LOW);
  rdf.length = SPI.transfer(0x00);
  rdf.cmd0 = SPI.transfer(0x00);
  rdf.cmd1 = SPI.transfer(0x00);
  for (int i=0; i<rdf.length; i++) {
    rdf.msg[i] = SPI.transfer(0x00);
  }
  digitalWrite(PIN_CS, HIGH);
  #endif
}

void ZNP::poll(ZNPDataFrame &rdf)
{
  #ifdef USE_SPI
  //poll
  //wait for srdy go low
  while(digitalRead(PIN_SRDY) == HIGH) {
    delay(10);
  }
  
  //srdy gone low
  digitalWrite(PIN_SS_MRDY, LOW);

  //spi send poll command
  //send poll spi zero
  digitalWrite(PIN_CS, LOW);
  SPI.transfer(0x00);
  SPI.transfer(0x00);
  SPI.transfer(0x00);
  digitalWrite(PIN_CS, HIGH);
  
  //wait for srdy go high
  while(digitalRead(PIN_SRDY) == LOW) {
    delay(10);
  }
  #endif
  this->read_response(rdf);
  
  #ifdef USE_SPI
  digitalWrite(PIN_SS_MRDY, HIGH);
  #endif
  //poll done
}

void ZNP::sreq(const ZNPDataFrame &sdf, ZNPDataFrame &rdf)
{
  #ifdef USE_SPI
  digitalWrite(PIN_SS_MRDY, LOW);
  while(digitalRead(PIN_SRDY) == HIGH) {
    delay(10);
  }

  //send spi data
  digitalWrite(PIN_CS, LOW);
  SPI.transfer(sdf.length);
  SPI.transfer(sdf.cmd0);
  SPI.transfer(sdf.cmd1);

  for(int i=0; i<sdf.length; i++) {
    SPI.transfer(sdf.msg[i]);
  }
  digitalWrite(PIN_CS, HIGH);

  while(digitalRead(PIN_SRDY) == LOW) {
    delay(10);
  }
  #endif

  #ifdef USE_UART
  uint8_t serial_msg[length + 2];
  serial_msg[0] = length + 2;
  serial_msg[1] = cmd0;
  serial_msg[2] = cmd1;
  for(int i=3;i<length;i++) {
    serial_msg[i] = msg[i];
  }

  digitalWrite(PIN_RTS, HIGH);
  while (digitalRead(PIN_CTS) != HIGH) {
    delay(10);
  }
  
  this->serial->write(serial_msg, length + 2);
  digitalWrite(PIN_RTS, LOW);
  #endif

  //receive data
  this->read_response(rdf);

  #ifdef USE_SPI
  digitalWrite(PIN_SS_MRDY, HIGH);
  #endif
}


void ZNP::areq(const ZNPDataFrame &sdf)
{
  #ifdef USE_SPI
  digitalWrite(PIN_SS_MRDY, LOW);
  while(digitalRead(PIN_SRDY) == HIGH) {
    delay(10);
  }
  
  //start data transmission
  digitalWrite(PIN_CS, LOW);
  SPI.transfer(sdf.length);
  SPI.transfer(sdf.cmd0);
  SPI.transfer(sdf.cmd1);

  for(int i=0; i<sdf.length; i++) {
    SPI.transfer(sdf.msg[i]);
  }
  digitalWrite(PIN_CS, HIGH);

  while(digitalRead(PIN_SRDY) == LOW) {
    delay(10);
  }

  digitalWrite(PIN_SS_MRDY, HIGH);
  #endif
}

#if 0
poll_response ZNP::set_config(uint8_t config_id, uint8_t len, uint8_t * msg)
{
  uint8_t length = 2 + len;
  uint8_t new_msg[length];
  new_msg[0] = config_id;
  new_msg[1] = len;
  for(int i=0;i<len;i++) {
    new_msg[i+2] = msg[i];
  }
  
  return this->sreq(length, 0x26, 0x05, new_msg);
}

poll_response ZNP::set_logical_type(uint8_t t)
{
  uint8_t msg[1];
  msg[0] = t;
  return this->set_config(ZCD_NV_LOGICAL_TYPE, 1, msg);
}

poll_response ZNP::set_pan_id(uint16_t pan_id) {
  uint8_t msg[2];
  msg[0] = 0xFF & pan_id;
  msg[1] = 0xFF & (pan_id >> 8);

  /*msg[1] = 0xFF & pan_id;
  msg[0] = 0xFF & (pan_id >> 8);*/

  //memcpy(msg, &pan_id, 2);

  return this->set_config(ZCD_NV_PAN_ID, 2, msg);
}

poll_response ZNP::set_chanlist(uint32_t chanlist) {
  uint8_t msg[4];
  
  msg[0] = 0xFF & chanlist;
  msg[1] = 0xFF & (chanlist >> 8);
  msg[2] = 0xFF & (chanlist >> 16);
  msg[3] = 0xFF & (chanlist >> 24);

  /*msg[3] = 0xFF & chanlist;
  msg[2] = 0xFF & (chanlist >> 8);
  msg[1] = 0xFF & (chanlist >> 16);
  msg[0] = 0xFF & (chanlist >> 24);*/

  //memcpy(msg, &chanlist, 4);

  return this->set_config(ZCD_NV_CHANLIST, 4, msg);
}

poll_response ZNP::app_register(uint8_t app_end_point, uint16_t app_profile_id)
{
  uint8_t msg[3];

  //memcpy(msg, &app_profile_id, 1);
  //memcpy(&msg[0], &app_profile_id, 2);
  
  
  msg[0] = app_end_point;
  msg[1] = 0xFF & app_profile_id;
  msg[2] = 0xFF & (app_profile_id >> 8);

  
  /*msg[2] = app_end_point;
  msg[1] = 0xFF & app_profile_id;
  msg[0] = 0xFF & (app_profile_id >> 8);*/

  return this->sreq(3, 0x26, 0x0A, msg);
}

poll_response ZNP::start_request()
{
  return this->sreq(0x00, 0x26, 0x00, NULL);
}
#endif