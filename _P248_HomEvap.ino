#include "_Plugin_Helper.h"
#ifdef USES_P248

// ####################################################################################################
// ############################# Plugin 248: HomEvap Adiabatic Cooler/Humidifier ######################
// ####################################################################################################
//
//  Written by Henri de Jong (espeasy@enri.nl),
//      with most code inspired by plugin 224: _P224_DDS238.ino

#ifndef USES_MODBUS
#error This code needs MODBUS library, it should be enabled in 'define_plugin_sets.h', or your 'custom.h'
#endif

/*
   Circuit wiring
    GPIO Setting 1 -> RX (RO) (D6)
    GPIO Setting 2 -> TX (DI) (D7)
    GPIO Setting 3 -> DE/RE pin for MAX485 (D5)
    Use 1kOhm in serie on datapins!
 */

#define PLUGIN_248
#define PLUGIN_ID_248 248
#define PLUGIN_NAME_248 "HomEvap Cooling/Humidifier [DEVELOPMENT]"
#define PLUGIN_VALUENAME1_248 "Humidity"
#define PLUGIN_VALUENAME2_248 "Temperature"
#define PLUGIN_VALUENAME3_248 "Temperature"
#define PLUGIN_VALUENAME4_248 "Temperature"
#define PLUGIN_VALUENAME5_248 "Humidity"
#define PLUGIN_VALUENAME6_248 "Mode"

#define P248_DEV_ID         PCONFIG(0)
#define P248_DEV_ID_LABEL   PCONFIG_LABEL(0)
#define P248_MODEL          PCONFIG(1)
#define P248_MODEL_LABEL    PCONFIG_LABEL(1)
#define P248_BAUDRATE       PCONFIG(2)
#define P248_BAUDRATE_LABEL PCONFIG_LABEL(2)
#define P248_QUERY1         PCONFIG(3)
#define P248_QUERY2         PCONFIG(4)
#define P248_QUERY3         PCONFIG(5)
#define P248_QUERY4         PCONFIG(6)
#define P248_QUERY5         PCONFIG(7)
#define P248_QUERY6         PCONFIG(8)
#define P248_DEPIN          CONFIG_PIN3

#define P248_NR_OUTPUT_VALUES   VARS_PER_TASK
#define P248_QUERY1_CONFIG_POS  3

#define P248_QUERY_AI3       0 // Duct Humidity
#define P248_QUERY_AI4       1 // Duct Temperature
#define P248_QUERY_AI5       2 // T2 temperature
#define P248_QUERY_SET_TEMP  3 // Duct temp setpoint
#define P248_QUERY_SET_HUMI  4 // Duct humi setpoint
#define P248_QUERY_SYS_MODE  5 // Control system mode
#define P248_NR_OUTPUT_OPTIONS  6 // Must be the last one

#define P248_DEV_ID_DFLT   1       // Modbus communication address
#define P248_MODEL_DFLT    0       // DDS238
#define P248_BAUDRATE_DFLT 3       // 9600 baud
#define P248_QUERY1_DFLT   P248_QUERY_AI3
#define P248_QUERY2_DFLT   P248_QUERY_AI4
#define P248_QUERY3_DFLT   P248_QUERY_AI5
#define P248_QUERY4_DFLT   P248_QUERY_SET_TEMP
#define P248_QUERY5_DFLT   P248_QUERY_SET_HUMI
#define P248_QUERY6_DFLT   P248_QUERY_SYS_MODE

#define P248_COMMAND_TEMP_SETPOINT 0    // Duct temperature setpoint
#define P248_COMMAND_HUMI_SETPOINT 1    // Duct humidity setpoint
#define P248_COMMAND_MODE 2             // System Mode

#define P248_MEASUREMENT_INTERVAL 60000L

#include <ESPeasySerial.h>
#include "src/Helpers/Modbus_RTU.h"

struct P248_data_struct : public PluginTaskData_base {
  P248_data_struct() {}

  ~P248_data_struct() {
    reset();
  }

  void reset() {
    modbus.reset();
  }

  bool init(ESPEasySerialPort port, const int16_t serial_rx, const int16_t serial_tx, int8_t dere_pin,
            unsigned int baudrate, uint8_t modbusAddress) {
    return modbus.init(port, serial_rx, serial_tx, baudrate, modbusAddress, dere_pin);
  }

  bool isInitialized() const {
    return modbus.isInitialized();
  }

  ModbusRTU_struct modbus;
};

unsigned int _plugin_248_last_measurement = 0;

boolean Plugin_248(byte function, struct EventStruct *event, String& string) {
  boolean success = false;

  switch (function) {
    case PLUGIN_DEVICE_ADD: {
      Device[++deviceCount].Number           = PLUGIN_ID_248;
      Device[deviceCount].Type               = DEVICE_TYPE_SERIAL_PLUS1; // connected through 3 datapins
      Device[deviceCount].VType              = Sensor_VType::SENSOR_TYPE_QUAD;
      Device[deviceCount].Ports              = 0;
      Device[deviceCount].PullUpOption       = false;
      Device[deviceCount].InverseLogicOption = false;
      Device[deviceCount].FormulaOption      = false;
      Device[deviceCount].ValueCount         = P248_NR_OUTPUT_VALUES;
      Device[deviceCount].SendDataOption     = true;
      Device[deviceCount].TimerOption        = true;
      Device[deviceCount].GlobalSyncOption   = true;
      break;
    }

    case PLUGIN_GET_DEVICENAME: {
      string = F(PLUGIN_NAME_248);
      break;
    }

    case PLUGIN_GET_DEVICEVALUENAMES: {
      for (byte i = 0; i < P248_NR_OUTPUT_VALUES; ++i) {
        if (i < P248_NR_OUTPUT_VALUES) {
          const byte pconfigIndex = i + P248_QUERY1_CONFIG_POS;
          byte choice             = PCONFIG(pconfigIndex);
          safe_strncpy(
            ExtraTaskSettings.TaskDeviceValueNames[i],
            Plugin_248_valuename(choice, false),
            sizeof(ExtraTaskSettings.TaskDeviceValueNames[i]));
        } else {
          ZERO_FILL(ExtraTaskSettings.TaskDeviceValueNames[i]);
        }
      }
      break;
    }

    case PLUGIN_GET_DEVICEGPIONAMES: {
      serialHelper_getGpioNames(event);
      event->String3 = formatGpioName_output_optional("DE");
      break;
    }

    case PLUGIN_WEBFORM_SHOW_CONFIG:
    {
      string += serialHelper_getSerialTypeLabel(event);
      success = true;
      break;
    }

    case PLUGIN_SET_DEFAULTS: {
      P248_DEV_ID   = P248_DEV_ID_DFLT;
      P248_MODEL    = P248_MODEL_DFLT;
      P248_BAUDRATE = P248_BAUDRATE_DFLT;
      P248_QUERY1   = P248_QUERY1_DFLT;
      P248_QUERY2   = P248_QUERY2_DFLT;
      P248_QUERY3   = P248_QUERY3_DFLT;
      P248_QUERY4   = P248_QUERY4_DFLT;
      P248_QUERY5   = P248_QUERY5_DFLT;
      P248_QUERY6   = P248_QUERY6_DFLT;

      success = true;
      break;
    }

    case PLUGIN_WEBFORM_SHOW_SERIAL_PARAMS:
    {
        String options_baudrate[4];

        for (int i = 0; i < 4; ++i) {
          options_baudrate[i] = String(p248_storageValueToBaudrate(i));
        }
        addFormSelector(F("Baud Rate"), P248_BAUDRATE_LABEL, 4, options_baudrate, NULL, P248_BAUDRATE);
        addUnit(F("baud"));
        addFormNumericBox(F("Modbus Address"), P248_DEV_ID_LABEL, P248_DEV_ID, 1, 247);
      break;
    }

    case PLUGIN_WEBFORM_LOAD: {
      P248_data_struct *P248_data =
        static_cast<P248_data_struct *>(getPluginTaskData(event->TaskIndex));

      if ((nullptr != P248_data) && P248_data->isInitialized()) {
        String detectedString = P248_data->modbus.detected_device_description;

        if (detectedString.length() > 0) {
          addFormNote(detectedString);
        }
        addRowLabel(F("Checksum (pass/fail/nodata)"));
        uint32_t reads_pass, reads_crc_failed, reads_nodata;
        P248_data->modbus.getStatistics(reads_pass, reads_crc_failed, reads_nodata);
        String chksumStats;
        chksumStats  = reads_pass;
        chksumStats += '/';
        chksumStats += reads_crc_failed;
        chksumStats += '/';
        chksumStats += reads_nodata;
        addHtml(chksumStats);

        addFormSubHeader(F("Logged Values"));
        p248_showValueLoadPage(P248_QUERY_AI3, event);
        p248_showValueLoadPage(P248_QUERY_AI4, event);
        p248_showValueLoadPage(P248_QUERY_AI5, event);
        p248_showValueLoadPage(P248_QUERY_SET_TEMP, event);
        p248_showValueLoadPage(P248_QUERY_SET_HUMI, event);
        p248_showValueLoadPage(P248_QUERY_SYS_MODE, event);
      }

      {
        // In a separate scope to free memory of String array as soon as possible
        sensorTypeHelper_webformLoad_header();
        String options[P248_NR_OUTPUT_OPTIONS];

        for (int i = 0; i < P248_NR_OUTPUT_OPTIONS; ++i) {
          options[i] = Plugin_248_valuename(i, true);
        }

        for (byte i = 0; i < P248_NR_OUTPUT_VALUES; ++i) {
          const byte pconfigIndex = i + P248_QUERY1_CONFIG_POS;
          sensorTypeHelper_loadOutputSelector(event, pconfigIndex, i, P248_NR_OUTPUT_OPTIONS, options);
        }
      }
      success = true;
      break;
    }

    case PLUGIN_WEBFORM_SAVE: {
      // Save normal parameters
      for (int i = 0; i < P248_QUERY1_CONFIG_POS; ++i) {
        pconfig_webformSave(event, i);
      }

      // Save output selector parameters.
      for (byte i = 0; i < P248_NR_OUTPUT_VALUES; ++i) {
        const byte pconfigIndex = i + P248_QUERY1_CONFIG_POS;
        const byte choice       = PCONFIG(pconfigIndex);
        sensorTypeHelper_saveOutputSelector(event, pconfigIndex, i, Plugin_248_valuename(choice, false));
      }

      success = true;
      break;
    }

    case PLUGIN_INIT: {
      const int16_t serial_rx = CONFIG_PIN1;
      const int16_t serial_tx = CONFIG_PIN2;
      const ESPEasySerialPort port = static_cast<ESPEasySerialPort>(CONFIG_PORT);
      initPluginTaskData(event->TaskIndex, new (std::nothrow) P248_data_struct());
      P248_data_struct *P248_data =
        static_cast<P248_data_struct *>(getPluginTaskData(event->TaskIndex));

      if (nullptr == P248_data) {
        return success;
      }

      if (P248_data->init(port, serial_rx, serial_tx, P248_DEPIN,
                          p248_storageValueToBaudrate(P248_BAUDRATE),
                          P248_DEV_ID)) {
        serialHelper_log_GpioDescription(port, serial_rx, serial_tx);
        success = true;
      } else {
        clearPluginTaskData(event->TaskIndex);
      }
      break;
    }

    case PLUGIN_EXIT: {
      success = true;
      break;
    }

    case PLUGIN_READ: {
      P248_data_struct *P248_data =
        static_cast<P248_data_struct *>(getPluginTaskData(event->TaskIndex));

      if ((nullptr != P248_data) && P248_data->isInitialized()) {
        for (int i = 0; i < P248_NR_OUTPUT_VALUES; ++i) {
          UserVar[event->BaseVarIndex + i] = p248_readValue(PCONFIG(i + P248_QUERY1_CONFIG_POS), event);
          delay(1);
        }

        success = true;
      }
      break;
    }

    case PLUGIN_WRITE: {
      String command = parseString(string, 1);

      if (command == F("homevap"))
        String subCommand = parseString(string, 2);

        if (subCommand == F("settemp"))
        {
          addLog(LOG_LEVEL_INFO, F("P248: Set duct command received"));
          int arg1 = parseString(string, 3).toFloat() * 10;
          p248_writeValue(P248_COMMAND_TEMP_SETPOINT, arg1, event);
          success = true;
        }
        else if (subCommand == F("sethumi"))
        {
          addLog(LOG_LEVEL_INFO, F("P248: Set humi command received"));
          int arg1 = parseString(string, 3).toFloat() * 10;
          p248_writeValue(P248_COMMAND_HUMI_SETPOINT, arg1, event);
          success = true;
        }
        else if (subCommand == F("setmode"))
        {
          addLog(LOG_LEVEL_INFO, F("P248: Set mode command received"));
          int arg1 = parseString(string, 3).toInt();
          p248_writeValue(P248_COMMAND_MODE, arg1, event);
          success = true;
        }

        break;
      }
    }
  }
  return success;
}

String Plugin_248_valuename(byte value_nr, bool displayString) {
  switch (value_nr) {
    case P248_QUERY_AI3: return displayString ? F("Duct humidity (%RH)") : F("Duct %RH");
    case P248_QUERY_AI4: return displayString ? F("Duct temperature (C)") : F("Duct C");
    case P248_QUERY_AI5: return displayString ? F("T2 temp (C)") : F("T2 C");
    case P248_QUERY_SET_TEMP: return displayString ? F("Duct temperature setpoint (C)") : F("Duct setpoint C");
    case P248_QUERY_SET_HUMI: return displayString ? F("Duct humidity setpoint (%RH)") : F("Duct setpoint %RH");
    case P248_QUERY_SYS_MODE: return displayString ? F("Control system mode") : F("Mode");
  }
  return "";
}

int p248_storageValueToBaudrate(byte baudrate_setting) {
  switch (baudrate_setting) {
    case 0:
      return 1200;
    case 1:
      return 2400;
    case 2:
      return 4800;
    case 3:
      return 9600;
  }
  return 9600;
}

float p248_readValue(byte query, struct EventStruct *event) {
  byte errorcode = -1;
  float value = 0;
  P248_data_struct *P248_data =
    static_cast<P248_data_struct *>(getPluginTaskData(event->TaskIndex));

  if ((nullptr != P248_data) && P248_data->isInitialized()) {
    switch (query) {
      case P248_QUERY_AI3:
        value = P248_data->modbus.readHoldingRegister(0x0F, errorcode) / 10.0; // 0.1 %RH => %RH
        break;
      case P248_QUERY_AI4:
        value = P248_data->modbus.readHoldingRegister(0x10, errorcode) / 10.0; // 0.1 C => C
        break;
      case P248_QUERY_AI5:
        value =  P248_data->modbus.readHoldingRegister(0x11, errorcode) / 10.0; // 0.1 C => C
        break;
      case P248_QUERY_SET_TEMP:
        value = P248_data->modbus.readHoldingRegister(0x2D, errorcode) / 10.0; // 0.1 C => C
        break;
      case P248_QUERY_SET_HUMI:
        value = P248_data->modbus.readHoldingRegister(0x37, errorcode) / 10.0; // 0.1 %RH => %RH
        break;
      case P248_QUERY_SYS_MODE:
        value = P248_data->modbus.readHoldingRegister(0x58, errorcode) * 1.0 ; // 1: Auto, 2:Hum, 3: Cool, 4: Off
        break;
    }
  }
  if (errorcode == 0) return value;
  return 0.0f;
}

void p248_writeValue(byte command, int registerValue, struct EventStruct *event) {
  P248_data_struct *P248_data =
         static_cast<P248_data_struct *>(getPluginTaskData(event->TaskIndex));
  if (nullptr != P248_data && P248_data->isInitialized()) {
    switch (command) {
      case P248_COMMAND_TEMP_SETPOINT:
        P248_data->modbus.writeMultipleRegisters(0x2D, registerValue);
        break;
      case P248_COMMAND_HUMI_SETPOINT:
        P248_data->modbus.writeMultipleRegisters(0x37, registerValue);
        break;
      case P248_COMMAND_MODE:
        P248_data->modbus.writeMultipleRegisters(0x58, registerValue);
        break;
    }
  }
}

void p248_showValueLoadPage(byte query, struct EventStruct *event) {
  addRowLabel(Plugin_248_valuename(query, true));
  addHtml(String(p248_readValue(query, event)));
}

#endif // USES_P248
