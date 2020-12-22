#ifdef USES_P164
//#######################################################################################################
//#################################### Plugin 164: MSswitch #############################################
//#######################################################################################################

// Februray 3, 2019 initial release
// 433MHZ Transmitters: http://www.rflink.nl/blog2/wiring
// Known supported auto-learning switches are Nexa, Prove, Telldus & Jula Anslutt

// List of commands:
// (1) MSswitch,<G,1-16>,<ON/OFF>

// Usage:
// (1): Turn on switches in group (eg. MSswitch,G,on)

#include <Socketswitch.h>

// Download library from https://github.com/mortensalomon/Socket_Switch

Socketswitch *Plugin_164_msswitch;

#define PLUGIN_164
#define PLUGIN_ID_164         99
#define PLUGIN_NAME_164       "MS auto-learning Switch"
#define PLUGIN_VALUENAME1_164 ""

unsigned long SocketAddress;
String log164;

boolean Plugin_164(byte function, struct EventStruct *event, String& string)
{
  boolean success = false;

  switch (function)
  {

    case PLUGIN_DEVICE_ADD:
    {
      Device[++deviceCount].Number = PLUGIN_ID_164;
      Device[deviceCount].Type = DEVICE_TYPE_SINGLE;
      Device[deviceCount].Ports = 0;
      Device[deviceCount].Custom = true;
      Device[deviceCount].TimerOption = false;
      Device[deviceCount].SendDataOption = false;
      break;
    }

    case PLUGIN_GET_DEVICENAME:
    {
      string = F(PLUGIN_NAME_164);
      break;
    }

    case PLUGIN_GET_DEVICEVALUENAMES:
    {
      strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[0], PSTR(PLUGIN_VALUENAME1_164));
      break;
    }

    case PLUGIN_WEBFORM_LOAD:
    {
      addRowLabel(F("433MHZ Data Pin"));
      addPinSelect(false, F("taskdevicepin1"), CONFIG_PIN1);
      addFormNumericBox(F("Switch Address"), F("p164_address"), PCONFIG_LONG(0),0,67108863);

      if (CONFIG_PIN1>-1) {
        addFormSubHeader(F("Try it"));
        String options[17] = { F("CH 1"),F("CH 2"),F("CH 3"),F("CH 4"),F("CH 5"),F("CH 6"),F("CH 7"),F("CH 8"),F("CH 9"),F("CH 10"),F("CH 11"),F("CH 12"),F("CH 13"),F("CH 14"),F("CH 15"),F("CH 16"),F("GROUP") };
        int optionValues[17] = { 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16 };
        String sOptions[2] = { F("On"),F("Off") };
        int sOptionsValues[2] = {1,0};

        addFormSelector(F("Channel"), F("channel"), 17, options, optionValues, 0);
        addFormSelector(F("State"), F("state"), 2, sOptions, sOptionsValues, 0);
        addFormCheckBox(F("Send on submit"), F("send"), false);
      }
      success = true;
      break;
    }

    case PLUGIN_WEBFORM_SAVE:
    {
      PCONFIG_LONG(0) = getFormItemInt(F("p164_address"));
      SocketAddress = PCONFIG_LONG(0);
      if (isFormItemChecked(F("send"))) {
        noInterrupts();
        log164 = F("Sending: ");
        noInterrupts();
        if (getFormItemInt(F("state"))) {
          if (getFormItemInt(F("channel"))==16) {
            Plugin_164_msswitch->groupOn(SocketAddress);
            log164 += F("GroupOn");
          } else {
            Plugin_164_msswitch->channelOn(SocketAddress,getFormItemInt(F("channel")));
            log164 += F("channelOn: ");
            log164 += getFormItemInt(F("channel"));
            log164 += F(" Address: ");
            log164 += SocketAddress;
            log164 += F(" PIN: ");
            log164 += CONFIG_PIN1;
          }
        } else {
          if (getFormItemInt(F("channel"))==16) {
            Plugin_164_msswitch->groupOff(SocketAddress);
            log164 += F("GroupOff");
          } else {
            Plugin_164_msswitch->channelOff(SocketAddress,getFormItemInt(F("channel")));
            log164 += F("channelOff: ");
            log164 += getFormItemInt(F("channel"));
          }
        }
        interrupts();
        addLog(LOG_LEVEL_DEBUG, log164);
      }
      success = true;
      break;
    }

    case PLUGIN_INIT:
    {
      if (!Plugin_164_msswitch)
      {
        Plugin_164_msswitch = new Socketswitch(CONFIG_PIN1);
        log164 = F("Init 433MHZ Pin: ");
        log164 += CONFIG_PIN1;
        addLog(LOG_LEVEL_DEBUG, log164);
      }
      //SocketAddress = PCONFIG(0);
      success = true;
      break;
    }

    case PLUGIN_WRITE:
    {
      if (Plugin_164_msswitch)
      {

        SocketAddress = PCONFIG_LONG(0);
        String tmpString  = string;
        if (tmpString.startsWith(F("MSswitch"))){
          noInterrupts();
          int idx1 = tmpString.indexOf(',');
          int idx2 = tmpString.indexOf(',', idx1+1);
          int idx3 = tmpString.indexOf(',', idx2+1);
          String val_dev = tmpString.substring(idx1+1, idx2);
          String val_state = tmpString.substring(idx2+1, idx3);
          if (val_dev.equalsIgnoreCase("g")) {
            if (val_state.equalsIgnoreCase("on")) Plugin_164_msswitch->groupOn(SocketAddress);
            if (val_state.equalsIgnoreCase("off")) Plugin_164_msswitch->groupOff(SocketAddress);
          } else {
            if (val_dev == "1" || val_dev == "2" || val_dev == "3" || val_dev == "4" || val_dev == "5" || val_dev == "6" || val_dev == "7" || val_dev == "8" || val_dev == "9" || val_dev == "10" || val_dev == "11" || val_dev == "12" || val_dev == "13" || val_dev == "14" || val_dev == "15" || val_dev == "16") {
              if (val_state.equalsIgnoreCase("on")) Plugin_164_msswitch->channelOn(SocketAddress,val_dev.toInt()-1);
              if (val_state.equalsIgnoreCase("off")) Plugin_164_msswitch->channelOff(SocketAddress,val_dev.toInt()-1);
            }
          }
          log164 = F("Sending to address: ");
          log164 += SocketAddress;
          addLog(LOG_LEVEL_DEBUG, log164);
          success = true;
          interrupts();
        }
      }
      break;
    }

  }
  return success;
}
#endif // USES_P164
