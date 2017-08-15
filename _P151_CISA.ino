//#######################################################################################################
//################################ Plugin 151: CISA door lock ###########################################
//#######################################################################################################
//############################### Plugin for ESP Easy by DatuX            ###############################
//################################### http://www.datux.nl  ##############################################
//#######################################################################################################

//green: door will unlock as soon as person touches door
//red:   door will stay locked as soon as person touches it.
//orange: you just closed the door, unlocking delay is active (otherwise it would keep unlocking)
//blinking: mechanical lock is unlocked: please close (or open+close) the door, or dont touch the door.


#ifdef PLUGIN_BUILD_TESTING

#define PLUGIN_151
#define PLUGIN_ID_151         151
#define PLUGIN_NAME_151       "Cisa door lock [TESTING]"

#ifndef CONFIG
#define CONFIG(n) (Settings.TaskDevicePluginConfig[event->TaskIndex][n])
#endif


bool Plugin_151_want_unlock=false;
unsigned long Plugin_151_last_unlock_time=0;
unsigned long Plugin_151_invert_time=0; //timestamp after which to automatcily invert unlock status.

//measure capicitance by pulsing out_pin and measuring response delay of in_pin
int Plugin_151_sense(byte out_pin, byte in_pin)
{
  long count=0;
  long max_pulses=30;
  long skip_pulses=20;

  pinMode(out_pin,OUTPUT);
  pinMode(in_pin, INPUT);


  unsigned long m;
  for (long pulses=0; pulses<max_pulses ; pulses++ )
  {
    // m=micros();
    m=0;
    digitalWrite(out_pin,0);
    // while (digitalRead(in_pin)!=0 && count<1000*max_pulses) count++;
    while (digitalRead(in_pin)!=0 && m<1000) m++;


    if (pulses>=skip_pulses)
      count=count+(m);

    delay(1); //let it further charge the capacitive load

    // m=micros();
    m=0;
    digitalWrite(out_pin,1);
    while (digitalRead(in_pin)!=1  && m<1000) m++;

    if (pulses>=skip_pulses)
      count=count+(m);


    delay(1); //let it further charge the capacitive load
  }
  // if (count>=1000*max_pulses)
  //   return(-1);

  return(count/ (max_pulses-skip_pulses));
}

//send unlock pulse
void Plugin_151_unlock(byte pin)
{
  pinMode(pin, OUTPUT);
  digitalWrite(pin,1);
  delay(100);
  digitalWrite(pin,0);
}


//use timer to blink pin with certain speed and dutycycle
void Plugin_151_blink(byte pin, unsigned long on_time, unsigned long total_time, bool on)
{
  pinMode(pin, OUTPUT);
  if (on)
  {
    digitalWrite(pin, (millis()%total_time)<on_time);
  }
  else
  {
    digitalWrite(pin, (millis()%total_time)< (total_time-on_time));
  }
}


boolean Plugin_151(byte function, struct EventStruct *event, String& string)
{
  boolean success = false;

  switch (function)
  {
    case PLUGIN_DEVICE_ADD:
    {
      Device[++deviceCount].Number = PLUGIN_ID_151;
      Device[deviceCount].Type = DEVICE_TYPE_TRIPLE;
      Device[deviceCount].VType = SENSOR_TYPE_SINGLE;
      Device[deviceCount].Ports = 0;
      Device[deviceCount].PullUpOption = false;
      Device[deviceCount].InverseLogicOption = false;
      Device[deviceCount].FormulaOption = false;
      Device[deviceCount].ValueCount = 1;
      Device[deviceCount].SendDataOption = true;
      Device[deviceCount].TimerOption = false;
      Device[deviceCount].GlobalSyncOption = false;
      break;
    }

    case PLUGIN_GET_DEVICENAME:
    {
      string = F(PLUGIN_NAME_151);
      break;
    }

    case PLUGIN_GET_DEVICEVALUENAMES:
    {
      break;
    }

    case PLUGIN_WEBFORM_LOAD:
    {
      addFormCheckBox(string, F("Invert led voltage"), F("led_invert"), CONFIG(4));

      addFormPinSelect(string, F("Sense out pin"), F("sense_out"), CONFIG(0));
      addFormPinSelect(string, F("Sense in pin"), F("sense_in"), CONFIG(1));

      addFormNumericBox(string, F("Sense locked minimum"), F("sense_locked"), CONFIG(2), 0, 65535);
      addFormNumericBox(string, F("Sense hand detect minimum"), F("sense_person"), CONFIG(3), 0, 65535);

      addFormNumericBox(string, F("Unlock delay"), F("unlock_delay"), CONFIG(5), 0, 65535);
      addUnit(string, F("ms"));


      success = true;
      break;
    }

    case PLUGIN_WEBFORM_SAVE:
    {
      CONFIG(0) = getFormItemInt(F("sense_out"));
      CONFIG(1) = getFormItemInt(F("sense_in"));
      CONFIG(2) = getFormItemInt(F("sense_locked"));
      CONFIG(3) = getFormItemInt(F("sense_person"));
      CONFIG(4) = isFormItemChecked(F("led_invert"));
      CONFIG(5) = getFormItemInt(F("unlock_delay"));

      success = true;
      break;
    }

    case PLUGIN_INIT:
    {

      success = true;
      break;
    }

    case PLUGIN_TEN_PER_SECOND:
    {
        int sense=Plugin_151_sense(CONFIG(0), CONFIG(1));
        byte unlock_coil_pin=Settings.TaskDevicePin1[event->TaskIndex];
        byte locked_led_pin=Settings.TaskDevicePin2[event->TaskIndex];
        byte unlocked_led_pin=Settings.TaskDevicePin3[event->TaskIndex];
        byte led_on=!CONFIG(4)   ;


        pinMode(locked_led_pin, OUTPUT);
        pinMode(unlocked_led_pin, OUTPUT);


        String log;
        log=log+F("lock : ");

        //activate the correct led (usually lock-led is red and unlocked led is green)
        byte active_led;
        if (Plugin_151_want_unlock)
        {
          log=log+F("UNLOCKED");
          active_led=unlocked_led_pin;
          digitalWrite(locked_led_pin,!led_on);
        }
        else
        {
          log=log+F("LOCKED");
          active_led=locked_led_pin;
          digitalWrite(unlocked_led_pin,!led_on);
        }

        //automaticly invert locking status after this time (usefull to temporary unlock a door)
        if (Plugin_151_invert_time)
        {
          if (now()>Plugin_151_invert_time)
          {
            Plugin_151_want_unlock=!Plugin_151_want_unlock;
            Plugin_151_invert_time=0;
          }
          else
          {
              log=log+F("(");
              log=log+(Plugin_151_invert_time-now());
              log=log+F("s )");
          }
        }


        log=log+F(" mechanism: sense=")+sense+F(", ");


        //STATE: mechanism locked
        if (sense>CONFIG(2))
        {
          log=log+F("locked");

          //person detected
          if (sense>CONFIG(3))
          {
            //allowed to unlock the door?
            if (Plugin_151_want_unlock)
            {
              //last time we unlocked long enough?
              if (millis()-Plugin_151_last_unlock_time>CONFIG(5))
              {
                //open the locking mechanism
                log=log+F(", person detected, opening");
                Plugin_151_unlock(unlock_coil_pin);
              }
              else
              {
                log=log+F(", person detected, delaying next open");
                Plugin_151_blink(locked_led_pin, 100,200, led_on);
                Plugin_151_blink(unlocked_led_pin, 100,200, led_on);
               }
            }
            else
            {
              //last time we unlocked long enough?
              if (millis()-Plugin_151_last_unlock_time>CONFIG(5))
              {
                //warn user the door is locked and cant be openend
                log=log+F(", person detected, denied access!");
                Plugin_151_blink(active_led, 100,200, led_on);
              }
              else
              {
                log=log+F(", person detected, delaying denied access.");
                digitalWrite(active_led, led_on);
              }
            }
          }
          //no person detected
          else
          {
            if (millis()-Plugin_151_last_unlock_time>CONFIG(5) || !Plugin_151_want_unlock)
            {
              //locking mechanism is locked, just activate the led.
              digitalWrite(active_led, led_on);
            }
            else
            {
              //delay is still active, inform user by making it orange.
              digitalWrite(locked_led_pin, led_on);
              digitalWrite(unlocked_led_pin, led_on);

            }
          }
        }

        //STATE: mechanism unlocked
        else
        {
          //locking mechanism is unlocked
          if (Plugin_151_want_unlock)
          {
            //good but inform user so they know its unlocked
            log=log+F("unlocked");
            Plugin_151_blink(active_led, 100,200, led_on);
          }
          else
          {
            //mechanism is unlocked but we want it to be locked again, warn user (they should close door or open+close it, depending on the state of the mechanism)
            log=log+F("unlocked, please close door!");
            Plugin_151_blink(active_led, 100,200, led_on);
          }

          Plugin_151_last_unlock_time=millis();
        }

        addLog(LOG_LEVEL_INFO, log);



    }


    case PLUGIN_GET_DEVICEGPIONAMES:
    {

      event->String1=F("Unlock coil pin");
      event->String2=F("Locked led pin");
      event->String3=F("Unlocked led pin");
      break;
    }


    case PLUGIN_WRITE:
    {

      String command = parseString(string, 1);

      if (command == F("cisa_unlock"))
      {
        Plugin_151_want_unlock=true;
        Plugin_151_invert_time=0;
        if (event->Par1)
          Plugin_151_invert_time=now()+event->Par1;

        success = true;
      }
      if (command == F("cisa_lock"))
      {
        Plugin_151_want_unlock=false;
        Plugin_151_invert_time=0;
        if (event->Par1)
          Plugin_151_invert_time=now()+event->Par1;
        success = true;
      }

      break;
    }

  }
  return success;
}

#endif
