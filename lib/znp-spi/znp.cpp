#include "znp.h"

#define DEFAULT_TIMEOUT_MS 1000

ZNPDataFrame::ZNPDataFrame(uint8_t len, uint8_t c0, uint8_t c1, uint8_t *m)
{
  this->length = len;
  this->cmd0 = c0;
  this->cmd1 = c1;
  this->msg = m;
}

ZNPDataFrame::ZNPDataFrame(uint8_t *m)
{
  this->length = 0;
  this->cmd0 = 0;
  this->cmd1 = 0;
  this->msg = m;
}

uint8_t ZNPDataFrame::get_length() const { return this->length; }
uint8_t ZNPDataFrame::get_cmd0() const { return this->cmd0; }
uint8_t ZNPDataFrame::get_cmd1() const { return this->cmd1; }
uint8_t* ZNPDataFrame::get_msg() const { return this->msg; }
void ZNPDataFrame::set_length(uint8_t d) { this->length = d; }
void ZNPDataFrame::set_cmd0(uint8_t d) { this->cmd0 = d; }
void ZNPDataFrame::set_cmd1(uint8_t d) { this->cmd1 = d; }

bool ZNP::wait_srdy_timeout(int target_level, int ms) {
  int i = ms;
  while(i && (digitalRead(PIN_SRDY) != target_level)) {
    delay(1);
    i--;
  }
  return (i > 0) ? true : false;
}

void ZNP::init()
{
  pinMode(PIN_SRDY, INPUT);
  pinMode(PIN_SS_MRDY, OUTPUT);
  pinMode(PIN_CS, OUTPUT);
  SPI.setBitOrder(MSBFIRST);
  if (SPI_MODE == 0)
    SPI.setDataMode(SPI_MODE0);
  else
    SPI.setDataMode(SPI_MODE3);
  SPI.setClockDivider(SPI_CLOCK_DIV8);
  digitalWrite(PIN_SS_MRDY, HIGH);
  digitalWrite(PIN_CS, HIGH);
  SPI.begin();
}

bool ZNP::available()
{
  // SRDY low means ZNP has data want to send to AP.
  return !digitalRead(PIN_SRDY);
}

int ZNP::read_response(ZNPDataFrame *rdf)
{
  int ret = 0;

  digitalWrite(PIN_CS, LOW);
  rdf->set_length(SPI.transfer(0x00));
  rdf->set_cmd0(SPI.transfer(0x00));
  rdf->set_cmd1(SPI.transfer(0x00));
  uint8_t *m = rdf->get_msg();
  for (int i=0; i<rdf->get_length(); i++) {
    m[i] = SPI.transfer(0x00);
    ret++;
  }
  digitalWrite(PIN_CS, HIGH);
  return ret;
}

int ZNP::poll(ZNPDataFrame *rdf)
{
  //poll
  //wait for srdy go low
  if (!wait_srdy_timeout(LOW, DEFAULT_TIMEOUT_MS))
    return -1;

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
  if (!wait_srdy_timeout(HIGH, DEFAULT_TIMEOUT_MS))
    return -1;

  int ret = this->read_response(rdf);

  digitalWrite(PIN_SS_MRDY, HIGH);
  //poll done
  return ret;
}

int ZNP::sreq(const ZNPDataFrame *sdf, ZNPDataFrame *rdf)
{
  digitalWrite(PIN_SS_MRDY, LOW);

  //wait for srdy go low
  if (!wait_srdy_timeout(LOW, DEFAULT_TIMEOUT_MS))
    return -1;

  //send spi data
  digitalWrite(PIN_CS, LOW);
  SPI.transfer(sdf->get_length());
  SPI.transfer(sdf->get_cmd0());
  SPI.transfer(sdf->get_cmd1());

  uint8_t *m = rdf->get_msg();
  for(int i=0; i<sdf->get_length(); i++) {
    SPI.transfer(m[i]);
  }
  digitalWrite(PIN_CS, HIGH);

  //wait for srdy go high
  if (!wait_srdy_timeout(HIGH, DEFAULT_TIMEOUT_MS))
    return -1;

  //receive data
  int ret = this->read_response(rdf);

  digitalWrite(PIN_SS_MRDY, HIGH);

  return ret;
}


void ZNP::areq(const ZNPDataFrame *sdf)
{
Serial.printf("len=0x%x cmd0=0x%x cmd1=0x%x m addr = 0x%x\n",
  sdf->get_length(), sdf->get_cmd0(), sdf->get_cmd1(), (unsigned int)sdf->get_msg());
Serial.printf("m[0]=0x%x\n", sdf->get_msg()[0]);
  digitalWrite(PIN_SS_MRDY, LOW);
  //wait for srdy go low
  if (!wait_srdy_timeout(LOW, DEFAULT_TIMEOUT_MS))
    return;
  
Serial.printf("znp areq send\n");
  //start data transmission
  digitalWrite(PIN_CS, LOW);
  SPI.transfer(sdf->get_length());
  SPI.transfer(sdf->get_cmd0());
  SPI.transfer(sdf->get_cmd1());
  uint8_t *m = sdf->get_msg();
  for(int i=0; i<sdf->get_length(); i++) {
    SPI.transfer(m[i]);
  }
  digitalWrite(PIN_CS, HIGH);

  //wait for srdy go high
  if (!wait_srdy_timeout(HIGH, DEFAULT_TIMEOUT_MS))
    return;

  digitalWrite(PIN_SS_MRDY, HIGH);
Serial.printf("znp areq done\n");
}
