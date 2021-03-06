/*
  xdrv_08_serial_bridge.ino - serial bridge support for Tasmota

  Copyright (C) 2020  Theo Arends and Dániel Zoltán Tolnai

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef USE_SERIAL_BRIDGE
/*********************************************************************************************\
 * Serial Bridge using Software Serial library (TasmotaSerial)
\*********************************************************************************************/

#define XDRV_08                    8

const uint8_t SERIAL_BRIDGE_BUFFER_SIZE = 130;

const char kSerialBridgeCommands[] PROGMEM = "|"  // No prefix
  D_CMND_SSERIALSEND "|" D_CMND_SBAUDRATE "|" D_CMND_SSER2NET_PORT;

void (* const SerialBridgeCommand[])(void) PROGMEM = {
  &CmndSSerialSend, &CmndSBaudrate, &CmndSSer2NetPort };

#include <TasmotaSerial.h>

TasmotaSerial *SerialBridgeSerial = nullptr;
WiFiServer *Ser2netServer = nullptr;
WiFiClient *Ser2netClient = nullptr;

unsigned long serial_bridge_polling_window = 0;
unsigned long serial_bridge_out_polling_window = 0;
char *serial_bridge_buffer = nullptr;
char *serial_bridge_out_buffer = nullptr;
int serial_bridge_in_byte_counter = 0;
int serial_bridge_out_byte_counter = 0;
bool serial_bridge_active = true;
bool serial_bridge_raw = false;

void SerialBridgeInput(void)
{
  while (SerialBridgeSerial->available()) {
    yield();
    uint8_t serial_in_byte = SerialBridgeSerial->read();
    // AddLog_P2(LOG_LEVEL_DEBUG_MORE, PSTR(D_LOG_DEBUG "Serial IN 0x%x"), serial_in_byte);
    if ((serial_in_byte > 127) && !serial_bridge_raw) {                        // Discard binary data above 127 if no raw reception allowed
      serial_bridge_in_byte_counter = 0;
      SerialBridgeSerial->flush();
      return;
    }
    if (serial_in_byte || serial_bridge_raw) {                                 // Any char between 1 and 127 or any char (0 - 255)

      if ((serial_bridge_in_byte_counter < SERIAL_BRIDGE_BUFFER_SIZE -1) &&    // Add char to string if it still fits and ...
          ((isprint(serial_in_byte) && (128 == Settings.serial_delimiter)) ||  // Any char between 32 and 127
          ((serial_in_byte != Settings.serial_delimiter) && (128 != Settings.serial_delimiter)) ||  // Any char between 1 and 127 and not being delimiter
            serial_bridge_raw)) {                                              // Any char between 0 and 255
        serial_bridge_buffer[serial_bridge_in_byte_counter++] = serial_in_byte;
        serial_bridge_polling_window = millis();                               // Wait for more data
      } else {
        serial_bridge_polling_window = 0;                                      // Publish now
        break;
      }
    }
  }

  if (serial_bridge_in_byte_counter && (millis() > (serial_bridge_polling_window + SERIAL_POLLING))) {
    serial_bridge_buffer[serial_bridge_in_byte_counter] = 0;                   // Serial data completed
    char hex_char[(serial_bridge_in_byte_counter * 2) + 2];
    bool assume_json = (!serial_bridge_raw && (serial_bridge_buffer[0] == '{'));
    if (Ser2netClient) {
      Ser2netClient->write(serial_bridge_buffer, serial_bridge_in_byte_counter);
    }
    Response_P(PSTR("{\"" D_JSON_SSERIALRECEIVED "\":%s%s%s}"),
      (assume_json) ? "" : "\"",
      (serial_bridge_raw) ? ToHex_P((unsigned char*)serial_bridge_buffer, serial_bridge_in_byte_counter, hex_char, sizeof(hex_char)) : serial_bridge_buffer,
      (assume_json) ? "" : "\"");
    MqttPublishPrefixTopic_P(RESULT_OR_TELE, PSTR(D_JSON_SSERIALRECEIVED));
    XdrvRulesProcess();
    serial_bridge_in_byte_counter = 0;
  }
}

void SerialBridgeOutput(void)
{
  if (Ser2netClient && !Ser2netClient->connected()) {
    Ser2netClient->stop();
    delete Ser2netClient;
    Ser2netClient = nullptr;
  }
  if (Ser2netServer && Ser2netServer->hasClient()) {
    // Free unconnected client first.
    if (Ser2netClient && !Ser2netClient->connected()) {
      Ser2netClient->stop();
      delete Ser2netClient;
      Ser2netClient = nullptr;
    }
    Ser2netClient = new WiFiClient;
    *Ser2netClient = Ser2netServer->available();
  }
  while (Ser2netClient && Ser2netClient->available()) {
    yield();
    uint8_t serial_out_byte = Ser2netClient->read();
    //AddLog_P2(LOG_LEVEL_DEBUG_MORE, PSTR(D_LOG_DEBUG "TCP IN 0x%x"), serial_out_byte);

    if ((serial_out_byte > 127) && !serial_bridge_raw) {                       // Discard binary data above 127 if no raw reception allowed
      serial_bridge_out_byte_counter = 0;
      Ser2netClient->flush();
      return;
    }
    if (serial_out_byte || serial_bridge_raw) {                                // Any char between 1 and 127 or any char (0 - 255)

      if ((serial_bridge_out_byte_counter < SERIAL_BRIDGE_BUFFER_SIZE -1) &&    // Add char to string if it still fits and ...
          ((isprint(serial_out_byte) && (128 == Settings.serial_delimiter)) ||  // Any char between 32 and 127
          ((serial_out_byte != Settings.serial_delimiter) && (128 != Settings.serial_delimiter)) ||  // Any char between 1 and 127 and not being delimiter
            serial_bridge_raw)) {                                              // Any char between 0 and 255
        serial_bridge_out_buffer[serial_bridge_out_byte_counter++] = serial_out_byte;
        serial_bridge_out_polling_window = millis();                               // Wait for more data
      } else {
        serial_bridge_out_polling_window = 0;                                      // Publish now
        break;
      }
    }
  }

  if (serial_bridge_out_byte_counter && (millis() > (serial_bridge_out_polling_window + SERIAL_POLLING))) {
    serial_bridge_out_buffer[serial_bridge_out_byte_counter] = 0;                   // Serial data completed
    char hex_char[(serial_bridge_out_byte_counter * 2) + 2];
    bool assume_json = (!serial_bridge_raw && (serial_bridge_out_buffer[0] == '{'));
    if (SerialBridgeSerial) {
      SerialBridgeSerial->write(serial_bridge_out_buffer, serial_bridge_out_byte_counter);
    }
    Response_P(PSTR("{\"" D_JSON_SSERIALTRANSMITED "\":%s%s%s}"),
      (assume_json) ? "" : "\"",
      (serial_bridge_raw) ? ToHex_P((unsigned char*)serial_bridge_out_buffer, serial_bridge_out_byte_counter, hex_char, sizeof(hex_char)) : serial_bridge_out_buffer,
      (assume_json) ? "" : "\"");
    MqttPublishPrefixTopic_P(RESULT_OR_TELE, PSTR(D_JSON_SSERIALRECEIVED));
    XdrvRulesProcess();
    serial_bridge_out_byte_counter = 0;
  }
}

/********************************************************************************************/

void SerialBridgeCreateTCPServer(int port) {
  if (port > 0) {
    serial_bridge_raw = true;
    Ser2netServer = new WiFiServer(Settings.ser2net_port);
    Ser2netServer->begin();
    Ser2netServer->setNoDelay(true);
    // create out buffer when Server create.
    serial_bridge_out_buffer = (char*)(malloc(SERIAL_BRIDGE_BUFFER_SIZE));
  } else {
    Ser2netServer->stop();
    delete Ser2netServer;
    Ser2netServer = nullptr;
    free(serial_bridge_out_buffer);
    serial_bridge_out_buffer = nullptr;
  }
}

void SerialBridgeInit(void)
{
  serial_bridge_active = false;
  if (PinUsed(GPIO_SBR_RX) && PinUsed(GPIO_SBR_TX)) {
    SerialBridgeSerial = new TasmotaSerial(Pin(GPIO_SBR_RX), Pin(GPIO_SBR_TX),
      (seriallog_level == LOG_LEVEL_NONE) ? 1 : 0);
    if (SerialBridgeSerial->begin(Settings.sbaudrate * 300)) {  // Baud rate is stored div 300 so it fits into 16 bits
      if (SerialBridgeSerial->hardwareSerial()) {
        ClaimSerial();
        serial_bridge_buffer = serial_in_buffer;  // Use idle serial buffer to save RAM
        AddLog_P2(LOG_LEVEL_DEBUG_MORE, PSTR(D_LOG_BRIDGE "Use Hardware Serial. Rx:%d Tx:%d"), Pin(GPIO_SBR_RX), Pin(GPIO_SBR_TX));
      } else {
        serial_bridge_buffer = (char*)(malloc(SERIAL_BRIDGE_BUFFER_SIZE));
      }
      serial_bridge_active = true;
      SerialBridgeSerial->flush();
    }
  }
}

/*********************************************************************************************\
 * Commands
\*********************************************************************************************/

void CmndSSerialSend(void)
{
  if ((XdrvMailbox.index > 0) && (XdrvMailbox.index <= 5)) {
    serial_bridge_raw = (XdrvMailbox.index > 3);
    if (XdrvMailbox.data_len > 0) {
      if (1 == XdrvMailbox.index) {
        SerialBridgeSerial->write(XdrvMailbox.data, XdrvMailbox.data_len);  // "Hello Tiger"
        SerialBridgeSerial->write("\n");                                    // "\n"
      }
      else if ((2 == XdrvMailbox.index) || (4 == XdrvMailbox.index)) {
        SerialBridgeSerial->write(XdrvMailbox.data, XdrvMailbox.data_len);  // "Hello Tiger" or "A0"
      }
      else if (3 == XdrvMailbox.index) {                                    // "Hello\f"
        SerialBridgeSerial->write(Unescape(XdrvMailbox.data, &XdrvMailbox.data_len), XdrvMailbox.data_len);
      }
      else if (5 == XdrvMailbox.index) {
        char *p;
        char stemp[3];
        uint8_t code;

        char *codes = RemoveSpace(XdrvMailbox.data);
        int size = strlen(XdrvMailbox.data);

        while (size > 1) {
          strlcpy(stemp, codes, sizeof(stemp));
          code = strtol(stemp, &p, 16);
          SerialBridgeSerial->write(code);                                  // "AA004566" as hex values
          size -= 2;
          codes += 2;
        }
      }
      ResponseCmndDone();
    }
  }
}

void CmndSBaudrate(void)
{
  if (XdrvMailbox.payload >= 300) {
    XdrvMailbox.payload /= 300;  // Make it a valid baudrate
    Settings.sbaudrate = XdrvMailbox.payload;
    SerialBridgeSerial->begin(Settings.sbaudrate * 300);  // Reinitialize serial port with new baud rate
  }
  ResponseCmndNumber(Settings.sbaudrate * 300);
}

void CmndSSer2NetPort(void)
{
  if (XdrvMailbox.payload > 0) {
    Settings.ser2net_port = XdrvMailbox.payload;
    if ((Ser2netServer) && (Ser2netServer->port() != Settings.ser2net_port))
      // Delete old server.
      SerialBridgeCreateTCPServer(-1);
    if (!Ser2netServer)
      SerialBridgeCreateTCPServer(Settings.ser2net_port);
  } else {
    SerialBridgeCreateTCPServer(-1);
    Settings.ser2net_port = 0;
  }
  ResponseCmndNumber(Settings.ser2net_port);
}

/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

bool Xdrv08(uint8_t function)
{
  bool result = false;

  if (serial_bridge_active) {
    switch (function) {
      case FUNC_LOOP:
        if (SerialBridgeSerial) { SerialBridgeInput(); }
        if (Settings.ser2net_port > 0) {
          if (Ser2netServer) {
            SerialBridgeOutput();
          } else {
            if (WiFi.status() == WL_CONNECTED)
              SerialBridgeCreateTCPServer(Settings.ser2net_port);
          }
        }
        break;
      case FUNC_PRE_INIT:
        SerialBridgeInit();
        break;
      case FUNC_COMMAND:
        result = DecodeCommand(kSerialBridgeCommands, SerialBridgeCommand);
        break;
    }
  }
  return result;
}

#endif // USE_SERIAL_BRIDGE
