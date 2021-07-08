//#######################################################################################################
//####################### Plugin 223: Input Switch with dallas one wire dummy ###########################
//### Perfect for using gpio 0 or gpio 2 in esp-01 as safe entries on restart if they are active. #######
//#################### Cut GND from ds18b20 (or other) with the switch. #################################
//#######################################################################################################

#define PLUGIN_223
#define PLUGIN_ID_223         223
#define PLUGIN_NAME_223       "Switch input - Dallas"
#define PLUGIN_VALUENAME1_223 "Switch"

uint8_t Plugin_223_DallasPin;

boolean Plugin_223(byte function, struct EventStruct *event, String& string)
{
  boolean success = false;
  static byte switchstate[TASKS_MAX];
  static byte outputstate[TASKS_MAX];

  switch (function)
  {
    case PLUGIN_DEVICE_ADD:
      {
        Device[++deviceCount].Number = PLUGIN_ID_223;
        Device[deviceCount].Type = DEVICE_TYPE_SINGLE;
        Device[deviceCount].VType = SENSOR_TYPE_SWITCH;
        Device[deviceCount].Ports = 0;
        Device[deviceCount].PullUpOption = false;
        Device[deviceCount].InverseLogicOption = false;
        Device[deviceCount].FormulaOption = false;
        Device[deviceCount].ValueCount = 1;
        Device[deviceCount].SendDataOption = true;
        Device[deviceCount].TimerOption = true;
        Device[deviceCount].TimerOptional = true;
        Device[deviceCount].GlobalSyncOption = true;
        break;
      }

    case PLUGIN_GET_DEVICENAME:
      {
        string = F(PLUGIN_NAME_223);
        break;
      }

    case PLUGIN_GET_DEVICEVALUENAMES:
      {
        strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[0], PSTR(PLUGIN_VALUENAME1_223));
        break;
      }

    case PLUGIN_WEBFORM_LOAD:
      {

        Plugin_223_DallasPin = Settings.TaskDevicePin1[event->TaskIndex];

        byte choice = Settings.TaskDevicePluginConfig[event->TaskIndex][0];

        String options[2];
        options[0] = F("Switch");
        options[1] = F("Dimmer");
        int optionValues[2] = { 1, 2 };
        addFormSelector(string, F("Switch Type"), F("plugin_223_type"), 2, options, optionValues, choice);

        if (Settings.TaskDevicePluginConfig[event->TaskIndex][0] == 2)
        {
          char tmpString[128];
          sprintf_P(tmpString, PSTR("<TR><TD>Dim value:<TD><input type='text' name='plugin_223_dimvalue' value='%u'>"), Settings.TaskDevicePluginConfig[event->TaskIndex][1]);
          string += tmpString;
        }

        choice = Settings.TaskDevicePluginConfig[event->TaskIndex][2];
        String buttonOptions[3];
        buttonOptions[0] = F("Normal Switch");
        buttonOptions[1] = F("Push Button Active Low");
        buttonOptions[2] = F("Push Button Active High");
        addFormSelector(string, F("Switch Button Type"), F("plugin_223_button"), 3, buttonOptions, NULL, choice);

        addFormCheckBox(string, F("Send Boot state"), F("plugin_223_boot"),
                        Settings.TaskDevicePluginConfig[event->TaskIndex][3]);

        success = true;
        break;
      }

    case PLUGIN_WEBFORM_SAVE:
      {

        Plugin_223_DallasPin = Settings.TaskDevicePin1[event->TaskIndex];

        Settings.TaskDevicePluginConfig[event->TaskIndex][0] = getFormItemInt(F("plugin_223_type"));
        if (Settings.TaskDevicePluginConfig[event->TaskIndex][0] == 2)
        {
          Settings.TaskDevicePluginConfig[event->TaskIndex][1] = getFormItemInt(F("plugin_223_dimvalue"));
        }

        Settings.TaskDevicePluginConfig[event->TaskIndex][2] = getFormItemInt(F("plugin_223_button"));

        Settings.TaskDevicePluginConfig[event->TaskIndex][3] = isFormItemChecked(F("plugin_223_boot"));

        success = true;
        break;
      }

    case PLUGIN_INIT:
      {
        /*
          if (Settings.TaskDevicePin1PullUp[event->TaskIndex])
          pinMode(Settings.TaskDevicePin1[event->TaskIndex], INPUT_PULLUP);
          else
          pinMode(Settings.TaskDevicePin1[event->TaskIndex], INPUT);

        */

        setPinState(PLUGIN_ID_223, Settings.TaskDevicePin1[event->TaskIndex], PIN_MODE_INPUT, 0);

        Plugin_223_DallasPin = Settings.TaskDevicePin1[event->TaskIndex];

        switchstate[event->TaskIndex] = Plugin_223_DS_reset();
        outputstate[event->TaskIndex] = switchstate[event->TaskIndex];

        // if boot state must be send, inverse default state
        if (Settings.TaskDevicePluginConfig[event->TaskIndex][3])
        {
          switchstate[event->TaskIndex] = !switchstate[event->TaskIndex];
          outputstate[event->TaskIndex] = !outputstate[event->TaskIndex];
        }
        success = true;
        break;
      }

    case PLUGIN_TEN_PER_SECOND:
      {
        byte state = Plugin_223_DS_reset();; //digitalRead(Settings.TaskDevicePin1[event->TaskIndex]);
        if (state != switchstate[event->TaskIndex])
        {
          switchstate[event->TaskIndex] = state;
          byte currentOutputState = outputstate[event->TaskIndex];

          if (Settings.TaskDevicePluginConfig[event->TaskIndex][2] == 0) //normal switch
            outputstate[event->TaskIndex] = state;
          else
          {
            if (Settings.TaskDevicePluginConfig[event->TaskIndex][2] == 1)  // active low push button
            {
              if (state == 0)
                outputstate[event->TaskIndex] = !outputstate[event->TaskIndex];
            }
            else  // active high push button
            {
              if (state == 1)
                outputstate[event->TaskIndex] = !outputstate[event->TaskIndex];
            }
          }

          // send if output needs to be changed
          if (currentOutputState != outputstate[event->TaskIndex])
          {
            byte sendState = outputstate[event->TaskIndex];
            if (Settings.TaskDevicePin1Inversed[event->TaskIndex])
              sendState = !outputstate[event->TaskIndex];
            UserVar[event->BaseVarIndex] = sendState;
            event->sensorType = SENSOR_TYPE_SWITCH;
            if ((sendState == 1) && (Settings.TaskDevicePluginConfig[event->TaskIndex][0] == 2))
            {
              event->sensorType = SENSOR_TYPE_DIMMER;
              UserVar[event->BaseVarIndex] = Settings.TaskDevicePluginConfig[event->TaskIndex][1];
            }
            String log = F("SW   : State ");
            log += sendState;
            addLog(LOG_LEVEL_INFO, log);
            sendData(event);
          }
        }
        success = true;
        break;
      }

    case PLUGIN_READ:
      {
        Plugin_223_DallasPin = Settings.TaskDevicePin1[event->TaskIndex];
        // We do not actually read the pin state as this is already done 10x/second
        // Instead we just send the last known state stored in Uservar
        String log = F("SW   : State ");
        log += UserVar[event->BaseVarIndex];
        addLog(LOG_LEVEL_INFO, log);
        success = true;
        break;
      }

    case PLUGIN_WRITE:
      {
        String log = "";
        String command = parseString(string, 1);

        if (command == F("gpioD"))
        {
          success = true;
          if (event->Par1 >= 0 && event->Par1 <= 16)
          {
            pinMode(event->Par1, OUTPUT);
            digitalWrite(event->Par1, event->Par2);
            setPinState(PLUGIN_ID_223, event->Par1, PIN_MODE_OUTPUT, event->Par2);
            log = String(F("SW   : GPIO ")) + String(event->Par1) + String(F(" Set to ")) + String(event->Par2);
            addLog(LOG_LEVEL_INFO, log);
            SendStatus(event->Source, getPinStateJSON(SEARCH_PIN_STATE, PLUGIN_ID_223, event->Par1, log, 0));
          }
        }

        if (command == F("pwmD"))
        {
          success = true;
          if (event->Par1 >= 0 && event->Par1 <= 16)
          {
            pinMode(event->Par1, OUTPUT);

            if (event->Par3 != 0)
            {
              byte prev_mode;
              uint16_t prev_value;
              getPinState(PLUGIN_ID_223, event->Par1, &prev_mode, &prev_value);
              if (prev_mode != PIN_MODE_PWM)
                prev_value = 0;

              int32_t step_value = ((event->Par2 - prev_value) << 12) / event->Par3;
              int32_t curr_value = prev_value << 12;

              int i = event->Par3;
              while (i--) {
                curr_value += step_value;
                int16_t new_value;
                new_value = (uint16_t)(curr_value >> 12);
                analogWrite(event->Par1, new_value);
                delay(1);
              }
            }

            analogWrite(event->Par1, event->Par2);
            setPinState(PLUGIN_ID_223, event->Par1, PIN_MODE_PWM, event->Par2);
            log = String(F("SW   : GPIO ")) + String(event->Par1) + String(F(" Set PWM to ")) + String(event->Par2);
            addLog(LOG_LEVEL_INFO, log);
            SendStatus(event->Source, getPinStateJSON(SEARCH_PIN_STATE, PLUGIN_ID_223, event->Par1, log, 0));
          }
        }

        if (command == F("pulseD"))
        {
          success = true;
          if (event->Par1 >= 0 && event->Par1 <= 16)
          {
            pinMode(event->Par1, OUTPUT);
            digitalWrite(event->Par1, event->Par2);
            delay(event->Par3);
            digitalWrite(event->Par1, !event->Par2);
            setPinState(PLUGIN_ID_223, event->Par1, PIN_MODE_OUTPUT, event->Par2);
            log = String(F("SW   : GPIO ")) + String(event->Par1) + String(F(" Pulsed for ")) + String(event->Par3) + String(F(" mS"));
            addLog(LOG_LEVEL_INFO, log);
            SendStatus(event->Source, getPinStateJSON(SEARCH_PIN_STATE, PLUGIN_ID_223, event->Par1, log, 0));
          }
        }

        if (command == F("longpulseD"))
        {
          success = true;
          if (event->Par1 >= 0 && event->Par1 <= 16)
          {
            pinMode(event->Par1, OUTPUT);
            digitalWrite(event->Par1, event->Par2);
            setPinState(PLUGIN_ID_223, event->Par1, PIN_MODE_OUTPUT, event->Par2);
            setSystemTimer(event->Par3 * 1000, PLUGIN_ID_223, event->Par1, !event->Par2, 0);
            log = String(F("SW   : GPIO ")) + String(event->Par1) + String(F(" Pulse set for ")) + String(event->Par3) + String(F(" S"));
            addLog(LOG_LEVEL_INFO, log);
            SendStatus(event->Source, getPinStateJSON(SEARCH_PIN_STATE, PLUGIN_ID_223, event->Par1, log, 0));
          }
        }

        if (command == F("statusD"))
        {
          if (parseString(string, 2) == F("gpio"))
          {
            success = true;
            SendStatus(event->Source, getPinStateJSON(SEARCH_PIN_STATE, PLUGIN_ID_223, event->Par2, dummyString, 0));
          }
        }

        if (command == F("inputswitchstateD"))
        {
          success = true;
          UserVar[event->Par1 * VARS_PER_TASK] = event->Par2;
          UserVar[event->Par1 * VARS_PER_TASK] = event->Par2;
          outputstate[event->Par1] = event->Par2;
        }

#ifdef PLUGIN_BUILD_TESTING
        //play a tune via a RTTTL string, look at https://www.letscontrolit.com/forum/viewtopic.php?f=4&t=343&hilit=speaker&start=10 for more info.
        if (command == F("rtttl"))
        {
          success = true;
          if (event->Par1 >= 0 && event->Par1 <= 16)
          {
            pinMode(event->Par1, OUTPUT);
            char sng[1024] = "";
            string.replace("-", "#");
            string.toCharArray(sng, 1024);
            play_rtttl(event->Par1, sng);
            setPinState(PLUGIN_ID_223, event->Par1, PIN_MODE_OUTPUT, event->Par2);
            log = String(F("SW   : ")) + string;
            addLog(LOG_LEVEL_INFO, log);
            SendStatus(event->Source, getPinStateJSON(SEARCH_PIN_STATE, PLUGIN_ID_223, event->Par1, log, 0));
          }
        }

        //play a tone on pin par1, with frequency par2 and duration par3.
        if (command == F("toneD"))
        {
          success = true;
          if (event->Par1 >= 0 && event->Par1 <= 16)
          {
            pinMode(event->Par1, OUTPUT);
            tone(event->Par1, event->Par2, event->Par3);
            setPinState(PLUGIN_ID_223, event->Par1, PIN_MODE_OUTPUT, event->Par2);
            log = String(F("SW   : ")) + string;
            addLog(LOG_LEVEL_INFO, log);
            SendStatus(event->Source, getPinStateJSON(SEARCH_PIN_STATE, PLUGIN_ID_223, event->Par1, log, 0));
          }
        }
#endif

        break;
      }

    case PLUGIN_TIMER_IN:
      {
        digitalWrite(event->Par1, event->Par2);
        setPinState(PLUGIN_ID_223, event->Par1, PIN_MODE_OUTPUT, event->Par2);
        break;
      }
  }
  return success;
}

/*********************************************************************************************\
   Dallas Reset and returns a 1/TRUE if there's a device present.
  \*********************************************************************************************/
uint8_t Plugin_223_DS_reset()
{
  uint8_t r;
  uint8_t retries = 125;
  // noInterrupts();
  pinMode(Plugin_223_DallasPin, INPUT);
  do // wait until the wire is high... just in case
  {
    if (--retries == 0)
      return 0;
    delayMicroseconds(2);
  }
  while (!digitalRead(Plugin_223_DallasPin));

  pinMode(Plugin_223_DallasPin, OUTPUT); digitalWrite(Plugin_223_DallasPin, LOW);
  delayMicroseconds(492);               // Dallas spec. = Min. 480uSec. Arduino 500uSec.
  pinMode(Plugin_223_DallasPin, INPUT); // Float
  delayMicroseconds(40);
  r = !digitalRead(Plugin_223_DallasPin);
  delayMicroseconds(420);
  // interrupts();
  return r;
}
