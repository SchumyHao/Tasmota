
#ifdef USE_CCLOADER

#define XDRV_98                    98

#include <LittleFS.h>
#include "CCLoader.h"

const char kCCloaderCommands[] PROGMEM = "|"  // No prefix
  D_CMND_CCLOADER_UPDATE;

void (* const CCloaderCommand[])(void) PROGMEM = {
  &CmndCCloaderUpdate };

static bool CCloader_active;
static CCLoader *loader = nullptr;
//static char fwName[] = "2530.bin";
static uint8_t spiffs_mounted=0;

/*
void SaveFile(const char *name,const uint8_t *buf,uint32_t len) {
  File file = SPIFFS.open(name, FILE_WRITE);
  if (!file) return;
  file.write(buf, len);
  file.close();
}


void LoadFile(const char *name,uint8_t *buf,uint32_t len) {
  if (!spiffs_mounted) {
    if(!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)){
          //Serial.println("SPIFFS Mount Failed");
      return;
    }
    spiffs_mounted=1;
  }
  File file = SPIFFS.open(name);
  if (!file) return;
  file.read(buf, len);
  file.close();
}
*/

static int CCloaderDoUpdate(const char* fwName)
{
  if (!spiffs_mounted) {
    if(!LittleFS.begin()) {
      AddLog_P(LOG_LEVEL_ERROR, PSTR(D_LOG_ZIGBEE "Mount SPIFFS failed"));
      return -1;
    }
    spiffs_mounted=1;
  }

  AddLog_P2(LOG_LEVEL_INFO, PSTR(D_LOG_ZIGBEE "Update cc253x fw %s"), fwName);
  File file = LittleFS.open(fwName, "r");
  if (!file) {
    AddLog_P2(LOG_LEVEL_ERROR, PSTR(D_LOG_ZIGBEE "Open %s failed"), fwName);
    return -1;
  }

  int blkTot = file.size() / 512;
  int remain = file.size() % 512;
  if (remain != 0) {
    blkTot++;
    AddLog_P(LOG_LEVEL_INFO, PSTR(D_LOG_ZIGBEE "!!WARNING: File's size isn't the integer multiples of 512 bytes, and the last block will be filled in up to 512 bytes with 0xFF!"));
  }
  AddLog_P2(LOG_LEVEL_DEBUG, PSTR(D_LOG_ZIGBEE "Block total: %d"), blkTot);

  // Do upload
  loader->ProgrammerInit();
  unsigned char chip_id = 0;
  unsigned char debug_config = 0;
  unsigned char verify = 0;

  loader->debug_init();
  chip_id = loader->read_chip_id();
  if (chip_id == 0) {
    AddLog_P(LOG_LEVEL_ERROR, PSTR(D_LOG_ZIGBEE "No chip detected, run loop again."));
    file.close();
    return -2;
  }
  loader->RunDUP();
  loader->debug_init();

  loader->chip_erase();
  loader->RunDUP();
  loader->debug_init();

  // Switch DUP to external crystal osc. (XOSC) and wait for it to be stable.
  // This is recommended if XOSC is available during programming. If
  // XOSC is not available, comment out these two lines.
  loader->write_xdata_memory(DUP_CLKCONCMD, 0x80);
  while (loader->read_xdata_memory(DUP_CLKCONSTA) != 0x80) {
    delay(1);
  }
  // Enable DMA (Disable DMA_PAUSE bit in debug configuration)
  debug_config = 0x22;
  loader->debug_command(CMD_WR_CONFIG, &debug_config, 1);

  // Program data (start address must be word aligned [32 bit])
  unsigned char rxBuf[512];
  unsigned char read_data[512];
  unsigned int addr = 0x0000;
  unsigned char bank;
  unsigned int offset;

  for (uint16_t i = 0; i < blkTot; i++) {
    AddLog_P2(LOG_LEVEL_DEBUG, PSTR(D_LOG_ZIGBEE "blkTot: %d"), i + 1);
    if ((i == (blkTot - 1)) && (remain != 0)) {
      file.read(rxBuf, remain);
      int filled = 512 - remain;
      for (uint16_t j = 0; j < filled; j++) {
        rxBuf[remain + j] = 0xFF;
      }
      AddLog_P(LOG_LEVEL_DEBUG, PSTR(D_LOG_ZIGBEE "last 0xFF"));
    } else {
      file.read(rxBuf, 512);
    }
    loader->write_flash_memory_block(rxBuf, addr, 512); // src, address, count
    if (verify) {
      bank = addr / (512 * 16);
      offset = (addr % (512 * 16)) * 4;
      loader->read_flash_memory_block(bank, offset, 512, read_data); // Bank, address, count, dest.
      for (uint16_t i = 0; i < 512; i++) {
        if (read_data[i] != rxBuf[i]) {
          // Fail
          loader->chip_erase();
          AddLog_P(LOG_LEVEL_ERROR, PSTR(D_LOG_ZIGBEE "Verify Error"));
          file.close();
          return -2;
        }
      }
    }
    addr += (unsigned int)128;
    delay(100);
  }
  file.close();
  loader->RunDUP();
  AddLog_P2(LOG_LEVEL_ERROR, PSTR(D_LOG_ZIGBEE "chip_id %d OK"), chip_id);
  delay(1000);
}

void CCloaderInit(void)
{
  CCloader_active = false;
  if (PinUsed(GPIO_CC_DC) && PinUsed(GPIO_CC_DD) && PinUsed(GPIO_CC_RST)) {
    loader = new CCLoader(Pin(GPIO_CC_DD), Pin(GPIO_CC_DC), Pin(GPIO_CC_RST));
    // Need or not?
    loader->ProgrammerInit();
    AddLog_P(LOG_LEVEL_INFO, PSTR(D_LOG_ZIGBEE "CCloader init OK"));
    CCloader_active = true;
  }
}

void CmndCCloaderUpdate(void)
{
  if (XdrvMailbox.data_len > 0) {
    CCloaderDoUpdate(XdrvMailbox.data);
  }
  ResponseCmndDone();
}

/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

bool Xdrv98(uint8_t function)
{
  bool result = false;

  switch (function) {
    //case FUNC_PIN_STATE:
    //  break;
    //case FUNC_LOOP:
    //  break;
    case FUNC_PRE_INIT:
      CCloaderInit();
      break;
    case FUNC_COMMAND:
      result = DecodeCommand(kCCloaderCommands, CCloaderCommand);
      break;
  }
  return result;
}

#endif // USE_CCLOADER