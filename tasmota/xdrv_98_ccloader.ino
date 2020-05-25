
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
static uint8_t spiffs_mounted=0;
static File fwfile;
static bool start_upload_loop;
static int blkTot;
static int remain;
static bool verify = true;

static int CCloaderDoUpdate(const char* fwName)
{
  if (!spiffs_mounted) {
    if(!LittleFS.begin()) {
      AddLog_P(LOG_LEVEL_ERROR, PSTR(D_LOG_ZIGBEE "Mount LittleFS failed"));
      return -1;
    }
    spiffs_mounted=1;
  }

  AddLog_P2(LOG_LEVEL_INFO, PSTR(D_LOG_ZIGBEE "Update cc253x fw %s"), fwName);

  if (fwfile)
    AddLog_P(LOG_LEVEL_INFO, PSTR(D_LOG_ZIGBEE "FW already opened"));
  else {
    fwfile = LittleFS.open(fwName, "r");
    if (!fwfile) {
      AddLog_P2(LOG_LEVEL_ERROR, PSTR(D_LOG_ZIGBEE "Open %s failed"), fwName);
      return -1;
    }
  }
  AddLog_P2(LOG_LEVEL_INFO, PSTR(D_LOG_ZIGBEE "fw size %d"), fwfile.size());

  blkTot = fwfile.size() / 512;
  remain = fwfile.size() % 512;
  if (remain != 0) {
    blkTot++;
    AddLog_P(LOG_LEVEL_INFO, PSTR(D_LOG_ZIGBEE "!!WARNING: File's size isn't the integer multiples of 512 bytes, and the last block will be filled in up to 512 bytes with 0xFF!"));
  }
  AddLog_P2(LOG_LEVEL_INFO, PSTR(D_LOG_ZIGBEE "Block total: %d"), blkTot);

  // Do upload
  loader->ProgrammerInit();
  unsigned char chip_id = 0;
  unsigned char debug_config = 0;

  loader->debug_init();
  chip_id = loader->read_chip_id();
  if (chip_id == 0) {
    AddLog_P(LOG_LEVEL_ERROR, PSTR(D_LOG_ZIGBEE "No chip detected, run loop again."));
    fwfile.close();
    return -2;
  }
  AddLog_P2(LOG_LEVEL_INFO, PSTR(D_LOG_ZIGBEE "ChipID: 0x%X"), chip_id);
  loader->RunDUP();
  loader->debug_init();

  loader->chip_erase();
  AddLog_P(LOG_LEVEL_INFO, PSTR(D_LOG_ZIGBEE "ChipID: Erase done"));
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

  AddLog_P(LOG_LEVEL_INFO, PSTR(D_LOG_ZIGBEE "Start uploader loop handler"));
  start_upload_loop = true;
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

void CCloaderLoop(void)
{
  static int blkIndex = 0;
  static unsigned int addr = 0x0000;
  // Program data (start address must be word aligned [32 bit])
  unsigned char rxBuf[512];
  unsigned char read_data[512];
  unsigned char bank;
  unsigned int offset;

  if (blkIndex < blkTot) {
    blkIndex++;
    AddLog_P2(LOG_LEVEL_INFO, PSTR(D_LOG_ZIGBEE "Start upload blk %d/%d"), blkIndex, blkTot);
    if ((blkIndex == (blkTot - 1)) && (remain != 0)) {
      fwfile.read(rxBuf, remain);
      int filled = 512 - remain;
      for (uint16_t j = 0; j < filled; j++) {
        rxBuf[remain + j] = 0xFF;
      }
      AddLog_P(LOG_LEVEL_DEBUG, PSTR(D_LOG_ZIGBEE "last 0xFF"));
    } else {
      fwfile.read(rxBuf, 512);
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
          fwfile.close();
          return;
        }
      }
    }
    addr += (unsigned int)128;
    AddLog_P2(LOG_LEVEL_INFO, PSTR(D_LOG_ZIGBEE "upload blk %d/%d done"), blkIndex, blkTot);
  } else {
    addr = 0x0000;
    blkIndex = 0;
    loader->RunDUP();
    AddLog_P(LOG_LEVEL_ERROR, PSTR(D_LOG_ZIGBEE "Firmware update OK"));
    fwfile.close();
    start_upload_loop = false;
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
    case FUNC_EVERY_100_MSECOND:
      if (start_upload_loop) CCloaderLoop();
      break;
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