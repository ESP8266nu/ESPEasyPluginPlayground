#ifdef USES_P112

//#######################################################################################################
//#################################### Plugin 112: Power Counter ########################################
//#######################################################################################################
// This sketch is based on Plugin 003: Pulse and the old no longer maintained Plugin 112: Power.

#include "_Plugin_Helper.h"

#define PLUGIN_112
#define PLUGIN_ID_112         112
#define PLUGIN_NAME_112       "Energy (AC) - Counter"
#define PLUGIN_VALUENAME1_112 "W"
#define PLUGIN_VALUENAME2_112 "Wh"

#define IndexDebounce         0
#define IndexPulsesPerkWh     1

void Plugin_112_pulse_interrupt1() ICACHE_RAM_ATTR;
void Plugin_112_pulse_interrupt2() ICACHE_RAM_ATTR;
void Plugin_112_pulse_interrupt3() ICACHE_RAM_ATTR;
void Plugin_112_pulse_interrupt4() ICACHE_RAM_ATTR;
void Plugin_112_pulsecheck(byte Index) ICACHE_RAM_ATTR;

// this takes 20 bytes of IRAM per handler
// void Plugin_112_pulse_interrupt5() ICACHE_RAM_ATTR;
// void Plugin_112_pulse_interrupt6() ICACHE_RAM_ATTR;
// void Plugin_112_pulse_interrupt7() ICACHE_RAM_ATTR;
// void Plugin_112_pulse_interrupt8() ICACHE_RAM_ATTR;

volatile unsigned long Plugin_112_pulseTotalCounter[TASKS_MAX];
volatile unsigned long Plugin_112_pulseTime[TASKS_MAX];
volatile unsigned long Plugin_112_bounceTimePrevious[TASKS_MAX];
volatile unsigned long Plugin_112_pulseTimePrevious[TASKS_MAX];
volatile float Plugin_112_pulseUsage[TASKS_MAX];

boolean Plugin_112(byte function, struct EventStruct *event, String& string)
{
  boolean success = false;

  switch (function)
  {
    case PLUGIN_DEVICE_ADD:
    {
      Device[++deviceCount].Number           = PLUGIN_ID_112;
      Device[deviceCount].Type               = DEVICE_TYPE_SINGLE;
      Device[deviceCount].VType              = Sensor_VType::SENSOR_TYPE_DUAL;
      Device[deviceCount].Ports              = 0;
      Device[deviceCount].PullUpOption       = false;
      Device[deviceCount].InverseLogicOption = false;
      Device[deviceCount].FormulaOption      = true;
      Device[deviceCount].ValueCount         = 2;
      Device[deviceCount].SendDataOption     = true;
      Device[deviceCount].TimerOption        = true;
      Device[deviceCount].GlobalSyncOption   = true;
      break;
    }

    case PLUGIN_GET_DEVICENAME:
    {
      string = F(PLUGIN_NAME_112);
      break;
    }

    case PLUGIN_GET_DEVICEVALUENAMES:
    {
      strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[0], PSTR(PLUGIN_VALUENAME1_112));
      strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[1], PSTR(PLUGIN_VALUENAME2_112));
      break;
    }

    case PLUGIN_GET_DEVICEGPIONAMES:
    {
      event->String1 = formatGpioName_input(F("Pulse"));
      break;
    }

    case PLUGIN_WEBFORM_LOAD:
    {
      addFormNumericBox(F("Debounce Time (mSec)"), F("p112_debouncetime")
                        , Settings.TaskDevicePluginConfig[event->TaskIndex][IndexDebounce]);

      // Let's prevent divison by zero
      if(Settings.TaskDevicePluginConfig[event->TaskIndex][IndexPulsesPerkWh]==0) {
        Settings.TaskDevicePluginConfig[event->TaskIndex][IndexPulsesPerkWh]=1000; // if not configged correctly prevent crashes and set it to 1000 as default value.
      }
      addFormNumericBox(F("Pulses per kWh"), F("p112_pulsesperkwh")
                        , Settings.TaskDevicePluginConfig[event->TaskIndex][IndexPulsesPerkWh]);

      success = true;
      break;
    }

    case PLUGIN_WEBFORM_SAVE:
    {
      Settings.TaskDevicePluginConfig[event->TaskIndex][IndexDebounce] = getFormItemInt(F("p112_debouncetime"));

      // Let's prevent divison by zero
      if(Settings.TaskDevicePluginConfig[event->TaskIndex][IndexPulsesPerkWh]==0) {
        Settings.TaskDevicePluginConfig[event->TaskIndex][IndexPulsesPerkWh]=1000; // if not configged correctly prevent crashes and set it to 1000 as default value.
      }
      Settings.TaskDevicePluginConfig[event->TaskIndex][IndexPulsesPerkWh] = getFormItemInt(F("p112_pulsesperkwh"));

      success = true;
      break;
    }

    case PLUGIN_WEBFORM_SHOW_VALUES:
    {
      addHtml(F("<div class=\"div_l\">"));
      addHtml(String(ExtraTaskSettings.TaskDeviceValueNames[0]));
      addHtml(F(":</div><div class=\"div_r\">"));
      addHtml(String(Plugin_112_pulseUsage[event->TaskIndex]));
      addHtml(F("</div><div class=\"div_br\"></div><div class=\"div_l\">"));
      addHtml(String(ExtraTaskSettings.TaskDeviceValueNames[1]));
      addHtml(F(":</div><div class=\"div_r\">"));

      // Let's prevent divison by zero
      if(Settings.TaskDevicePluginConfig[event->TaskIndex][IndexPulsesPerkWh]==0) {
        Settings.TaskDevicePluginConfig[event->TaskIndex][IndexPulsesPerkWh]=1000; // if not configged correctly prevent crashes and set it to 1000 as default value.
      }
      addHtml(String(Plugin_112_pulseTotalCounter[event->TaskIndex] * 1000 / Settings.TaskDevicePluginConfig[event->TaskIndex][IndexPulsesPerkWh]));
      addHtml(F("</div>"));

      success = true;
      break;
    }

    case PLUGIN_INIT:
    {
      // Restore any value that may have been read from the RTC.
      Plugin_112_pulseUsage[event->TaskIndex]        = UserVar[event->BaseVarIndex];
      // String log = F("INIT : Load ");
      // log += LoadCustomTaskSettings(event->TaskIndex, (byte*)&Plugin_112_pulseUsage[event->TaskIndex], sizeof(Plugin_112_pulseUsage[event->TaskIndex]));
      // addLog(LOG_LEVEL_INFO, log);
      Plugin_112_pulseTime[event->TaskIndex]         = UserVar[event->BaseVarIndex + 2];

      // Restore the total counter from the unused 4th UserVar value.
      // It may be using a formula to generate the output, which makes it impossible to restore
      // the true internal state.
      Plugin_112_pulseTotalCounter[event->TaskIndex] = UserVar[event->BaseVarIndex + 3];

      String log = F("INIT : Power ");
      log += Settings.TaskDevicePin1[event->TaskIndex];
      addLog(LOG_LEVEL_INFO, log);
      pinMode(Settings.TaskDevicePin1[event->TaskIndex], INPUT_PULLUP);
      success = Plugin_112_pulseinit(Settings.TaskDevicePin1[event->TaskIndex], event->TaskIndex);

      break;
    }

    case PLUGIN_READ:
    {
      Plugin_112_idleusage(event->TaskIndex);

      UserVar[event->BaseVarIndex]     = floor(Plugin_112_pulseUsage[event->TaskIndex] + 0.5);
      UserVar[event->BaseVarIndex + 1] = Plugin_112_pulseTotalCounter[event->TaskIndex] * 1000 /
        Settings.TaskDevicePluginConfig[event->TaskIndex][IndexPulsesPerkWh];
      UserVar[event->BaseVarIndex + 2] = Plugin_112_pulseTime[event->TaskIndex];

      // Store the raw value in the unused 4th position.
      // This is needed to restore the value from RTC as it may be converted into another output value using a formula.
      UserVar[event->BaseVarIndex + 3] = Plugin_112_pulseTotalCounter[event->TaskIndex];
      event->sensorType                = Sensor_VType::SENSOR_TYPE_DUAL;

      // String log = F("READ : Save ");
      // log += SaveCustomTaskSettings(event->TaskIndex, (byte*)&Plugin_112_pulseUsage[event->TaskIndex], sizeof(Plugin_112_pulseUsage[event->TaskIndex]));
      // addLog(LOG_LEVEL_INFO, log);

      success                          = true;
      break;
    }
  }

  return success;
}


/*********************************************************************************************\
 * Update usage when no pulse has been received for some time, so it will decrease on every
 * PLUGIN_READ event instead off keeping the last calculated usage value.
\*********************************************************************************************/
void Plugin_112_idleusage(byte Index)
{
  unsigned long PulseTime = millis() - Plugin_112_pulseTimePrevious[Index];
  if(PulseTime > (Settings.TaskDeviceTimer[Index] * 1000) &&  // More than $device_delay passed since last pulse
     Plugin_112_pulseTimePrevious[Index] > 0 &&               // Must have at least one pulse received
     PulseTime > Plugin_112_pulseTime[Index] ) {              // More than last pulse interval
    // WH = 3600000 / [pulses per kwh] / [time since last pulse (ms)]
    Plugin_112_pulseUsage[Index] = (3600000000. / Settings.TaskDevicePluginConfig[Index][IndexPulsesPerkWh]) / PulseTime;
  }
}


/*********************************************************************************************\
* Check Pulse Counters (called from irq handler)
\*********************************************************************************************/
void Plugin_112_pulsecheck(byte Index)
{
  noInterrupts(); // s0170071: avoid nested interrups due to bouncing.
  // s0170071: the following gives a glitch if millis() rolls over (every 50 days) and there is a bouncing to be avoided at the exact same
  // time. Very rare.
  // Alternatively there is timePassedSince(Plugin_112_bounceTimePrevious[Index]); but this is not in IRAM at this time, so do not use in a
  // ISR!
  const unsigned long now = millis(); // remember time here

  // Debounce on both edges:
  if ((now - Plugin_112_bounceTimePrevious[Index]) > (unsigned long)Settings.TaskDevicePluginConfig[Index][IndexDebounce])
  {
    Plugin_112_bounceTimePrevious[Index] = now;

    if (!digitalRead(Settings.TaskDevicePin1[Index])) {
      // only update readings on falling edge
      const unsigned long PulseTime       = now - Plugin_112_pulseTimePrevious[Index];
      Plugin_112_pulseTimePrevious[Index] = now;
      Plugin_112_pulseTime[Index]         = PulseTime;
      Plugin_112_pulseTotalCounter[Index]++;
      // WH = 3600000 / [pulses per kwh] / [time since last pulse (ms)]
      Plugin_112_pulseUsage[Index]        = (3600000000. / Settings.TaskDevicePluginConfig[Index][IndexPulsesPerkWh]) / PulseTime;
    }
  }
  interrupts(); // enable interrupts again.
}

/*********************************************************************************************\
* Pulse Counter IRQ handlers
\*********************************************************************************************/
void Plugin_112_pulse_interrupt1()
{
  Plugin_112_pulsecheck(0);
}

void Plugin_112_pulse_interrupt2()
{
  Plugin_112_pulsecheck(1);
}

void Plugin_112_pulse_interrupt3()
{
  Plugin_112_pulsecheck(2);
}

void Plugin_112_pulse_interrupt4()
{
  Plugin_112_pulsecheck(3);
}

void Plugin_112_pulse_interrupt5()
{
  Plugin_112_pulsecheck(4);
}

void Plugin_112_pulse_interrupt6()
{
  Plugin_112_pulsecheck(5);
}

void Plugin_112_pulse_interrupt7()
{
  Plugin_112_pulsecheck(6);
}

void Plugin_112_pulse_interrupt8()
{
  Plugin_112_pulsecheck(7);
}

/*********************************************************************************************\
* Init Pulse Counters
\*********************************************************************************************/
bool Plugin_112_pulseinit(byte Par1, byte Index)
{
  switch (Index)
  {
    // use CHANGE and not FALLING for correct debouncing and divide result by 2. See also https://github.com/letscontrolit/ESPEasy/issues/2366
    case 0:
      attachInterrupt(Par1, Plugin_112_pulse_interrupt1, CHANGE);
      break;
    case 1:
      attachInterrupt(Par1, Plugin_112_pulse_interrupt2, CHANGE);
      break;
    case 2:
      attachInterrupt(Par1, Plugin_112_pulse_interrupt3, CHANGE);
      break;
    case 3:
      attachInterrupt(Par1, Plugin_112_pulse_interrupt4, CHANGE);
      break;

    // case 4:
    //   attachInterrupt(Par1, Plugin_112_pulse_interrupt5, Mode);
    //   break;
    // case 5:
    //   attachInterrupt(Par1, Plugin_112_pulse_interrupt6, Mode);
    //   break;
    // case 6:
    //   attachInterrupt(Par1, Plugin_112_pulse_interrupt7, Mode);
    //   break;
    // case 7:
    //   attachInterrupt(Par1, Plugin_112_pulse_interrupt8, Mode);
    //   break;
    default:
      addLog(LOG_LEVEL_ERROR, F("POWER: Error, only the first 4 tasks can be pulse counters."));
      return false;
  }

  return true;
}

#endif // USES_P112
