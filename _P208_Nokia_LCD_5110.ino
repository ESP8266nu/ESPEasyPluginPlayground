//#######################################################################################################
//################################## Plugin 208: NOKIA 5110 lcd #########################################
//#######################################################################################################
#include "_Plugin_Helper.h"
#ifdef USES_P208

// Working:
// - support different digit-size's.
// - Display Text via:
//   1.  ESPEasy-Webinterface (Line-1-Line-x)
//   2.  http-request:
//      - BackLight on  via httpcmd (http://ESP-IP/control?cmd=pcd8544cmd,blOn)
//      - BackLight off via httpcmd (http://ESP-IP/control?cmd=pcd8544cmd,blOff)
//      - Clear Display via httpcmd (http://ESP-IP/control?cmd=pcd8544cmd,clear)
//      - Send Text via httpcmd     (http://ESP-IP/control?cmd=pcd8544,1,Hello World!)  // 1 : line#
//        Send Text via http-request only works for the empty lines in ESPEasy-Webinterface!
// - Pin connections:
//       SPI interface:
//       Example of tested configuration:
//       RST  -      => LCD_pin_1 reset connected to Vcc with 10k resistor
//       CE  GPIO-5  => LCD_pin_2 chip select
//       DC  GPIO-32 => LCD_pin_3 Data/Command select
//       DIN GPIO-23 => LCD_pin_4 Serial data 
//       CLK GPIO-18 => LCD_pin_5 Serial clock out
//       In hardware tab; 
//       - enable SPI; 
//       - Select VSPI:CLK=GPIO-18, MISO=GPIO-19, MOSI=GPIO-23 
//         (MISO not used)
//
// - Tested on
//    - Hardware ESP32
//    - ESPEasy-mega-20210223

// ToDo:
// - different digit-size within a line....
// - Choice of usable fonts on web-webform.
//
// Hardware note:
// It's often seen that pins are connected via (10k) risistors.
// Sometimes a screen will not work with these risistors. In that case, connect the display without resistors. 
// That works well. However, I have no long-term experience with this.

#define PLUGIN_208
#define PLUGIN_ID_208 208
#define PLUGIN_NAME_208 "Display - LCD PCD8544 (Nokia 5110) [Testing]"
#define PLUGIN_VALUENAME1_208 "Backlight"
#define PLUGIN_VALUENAME2_208 "Contrast"
#define PLUGIN_VALUENAME3_208 "Rotation"

#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>
#define lcd_lines 6
#define Digits_per_template_line 48  // This value must be used at the function "displayText" declaration!
#define digits_per_display_line 14

Adafruit_PCD8544 *lcd3 = nullptr;

char html_input [lcd_lines][digits_per_display_line];

boolean Plugin_208(byte function, struct EventStruct *event, String& string){
  boolean success = false;

  switch (function)
  {
    case PLUGIN_DEVICE_ADD:{
        Device[++deviceCount].Number = PLUGIN_ID_208 ;
        Device[deviceCount].Type = DEVICE_TYPE_SPI3;
        Device[deviceCount].VType = Sensor_VType::SENSOR_TYPE_SINGLE;
        Device[deviceCount].Ports = 0;
        Device[deviceCount].PullUpOption = false;
        Device[deviceCount].InverseLogicOption = false;
        Device[deviceCount].FormulaOption = false;
        Device[deviceCount].ValueCount = 3;
        Device[deviceCount].SendDataOption = false;
        Device[deviceCount].TimerOption = true;
        Device[deviceCount].TimerOptional = true;
        break;
      }

    case PLUGIN_GET_DEVICEGPIONAMES:      {
        event->String1 = formatGpioName_output(F("LCD CE pin 2"));
        event->String2 = formatGpioName_output(F("LCD DC pin 3"));
        event->String3 = formatGpioName_output(F("LCD BL pin 7"));
        break;
      }

    case PLUGIN_GET_DEVICENAME:{
        string = F(PLUGIN_NAME_208);
        break;
      }

    case PLUGIN_GET_DEVICEVALUENAMES:{
        strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[0], PSTR(PLUGIN_VALUENAME1_208));
        strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[1], PSTR(PLUGIN_VALUENAME2_208));
        strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[2], PSTR(PLUGIN_VALUENAME3_208));
        break;
      }

    case PLUGIN_WEBFORM_LOAD:{
        addFormNumericBox(F("Display Contrast(50-100):"), F("plugin_208_contrast"), PCONFIG(1));
        
        int optionValues3[4] = { 0, 1, 2, 3 };
        String options3[4] = { F("0"), F("90"), F("180"), F("270") };
        addFormSelector(F("Display Rotation"), F("plugin_208_rotation"), 4, options3, optionValues3, PCONFIG(2));

        String options4[2] = { F("OFF"), F("ON") };
        int optionValues4[2] = { 0, 1 };
        addFormSelector(F("Backlight"), F("plugin_208_backlight"), 2, options4, optionValues4, PCONFIG(0));

        int optionValues5[3] = { 1,2,3 };
        String options5[3] = { F("normal"), F("large"), F("x-large") };
        addFormSelector(F("Char.size line-1"), F("plugin_208_charsize_line_1"), 3, options5, optionValues5, PCONFIG(3)); 
        addFormSelector(F("Char.size line-2"), F("plugin_208_charsize_line_2"), 3, options5, optionValues5, PCONFIG(4)); 
        addFormSelector(F("Char.size line-3"), F("plugin_208_charsize_line_3"), 3, options5, optionValues5, PCONFIG(5)); 

        char deviceTemplate [lcd_lines][Digits_per_template_line];
        LoadCustomTaskSettings(event->TaskIndex, (byte*)&deviceTemplate, sizeof(deviceTemplate));
        for (byte varNr = 0; varNr < lcd_lines; varNr++)
        {
          addFormTextBox(String(F("Line ")) + (varNr + 1), String(F("Plugin_208_template")) + (varNr + 1), deviceTemplate[varNr], 80);
        }
        success = true;
        break;
      }

    case PLUGIN_WEBFORM_SAVE:{
        PCONFIG(0)= getFormItemInt(F("plugin_208_backlight"));
        PCONFIG(1)= getFormItemInt(F("plugin_208_contrast"));
        PCONFIG(2)= getFormItemInt(F("plugin_208_rotation"));
        PCONFIG(3)= getFormItemInt(F("plugin_208_charsize_line_1"));
        PCONFIG(4)= getFormItemInt(F("plugin_208_charsize_line_2"));
        PCONFIG(5)= getFormItemInt(F("plugin_208_charsize_line_3"));
        PCONFIG(6)= getFormItemInt(F("plugin_208_GPIO_CE"));
        PCONFIG(7)= getFormItemInt(F("plugin_208_GPIO_DC"));

        char deviceTemplate[lcd_lines][Digits_per_template_line];
        for (byte varNr = 0; varNr < lcd_lines; varNr++)
        {
          char argc[25];
          String arg = F("Plugin_208_template");
          arg += varNr + 1;
          arg.toCharArray(argc, 25);
          String tmpString = web_server.arg(argc);
          strncpy(deviceTemplate[varNr], tmpString.c_str(), sizeof(deviceTemplate[varNr]));
        }
        SaveCustomTaskSettings(event->TaskIndex, (byte*)&deviceTemplate, sizeof(deviceTemplate));
        success = true;
        break;
      }

    case PLUGIN_INIT:{
        if (!lcd3)
        {
          lcd3 = new Adafruit_PCD8544(Settings.TaskDevicePin2[event->TaskIndex], Settings.TaskDevicePin1[event->TaskIndex], -1); // Adafruit_PCD8544 (DC, CE, -1)
        }
        // Setup lcd3 display
        byte plugin1 = PCONFIG(2); // rotation
        byte plugin2 = PCONFIG(1); // contrast
        byte plugin4 = PCONFIG(0); // backlight_onoff
        UserVar[event->BaseVarIndex+2]=plugin1;
        UserVar[event->BaseVarIndex+1]=plugin2;
        UserVar[event->BaseVarIndex]=! plugin4;
        lcd3->begin();
        lcd3->setContrast(30);
        lcd3->setContrast(plugin2);
        lcd3->setRotation(plugin1);
        char deviceTemplate[lcd_lines][Digits_per_template_line];
        LoadCustomTaskSettings(event->TaskIndex, (byte*)&deviceTemplate, sizeof(deviceTemplate));
        displayText(deviceTemplate, event);
        //displayText((byte**)&deviceTemplate, event);
        lcd3->display();
        setBacklight(event); 
        success = true;
        break;
      }

    case PLUGIN_READ:{
        char deviceTemplate[lcd_lines][Digits_per_template_line];
        LoadCustomTaskSettings(event->TaskIndex, (byte*)&deviceTemplate, sizeof(deviceTemplate));
        displayText(deviceTemplate, event);
        //displayText((byte**)&deviceTemplate, event);
        success = false;
        break;
      }

    case PLUGIN_WRITE:{
        String tmpString  = string;
        String StringToDisplay;
        String line_content_ist;
        String line_content_soll;
        int argIndex = tmpString.indexOf(',');

        if (argIndex){
          tmpString = tmpString.substring(0, argIndex);
          if (tmpString.equalsIgnoreCase(F("PCD8544"))){
            if (event->Par1 <= 6 ){ // event->Par1 = row. 
              success = true;
              argIndex = string.lastIndexOf(',');
              char deviceTemplate [lcd_lines][Digits_per_template_line];
              LoadCustomTaskSettings(event->TaskIndex, (byte*)&deviceTemplate, sizeof(deviceTemplate));
              String linedefinition  = deviceTemplate[event->Par1-1];
              if (!linedefinition.length()){  // only if value is not definde in plugin-webform
                line_content_ist = html_input[event->Par1-1];
                line_content_soll = string.substring(argIndex + 1);
                if (line_content_soll.length() < line_content_ist.length() ) {
                  for(int i=line_content_soll.length(); i < line_content_ist.length(); i++){
                    line_content_soll += " ";
                  }
                }
                //addLog(LOG_LEVEL_INFO, "5:"+ line_content_soll);
                strncpy(html_input[event->Par1-1], line_content_soll.c_str(), sizeof(deviceTemplate[event->Par1-1]));
              }
            }
          }
          if (tmpString.equalsIgnoreCase(F("PCD8544CMD"))){
            success = true;
            argIndex = string.lastIndexOf(',');
            tmpString = string.substring(argIndex + 1);
            if (tmpString.equalsIgnoreCase(F("Clear"))){
              addLog(LOG_LEVEL_INFO, F("Clear Display"));
              lcd3->clearDisplay();
              lcd3->display();
            }
            if (tmpString.equalsIgnoreCase(F("blOn"))){
              success = true;
              PCONFIG(0) = 1;
              setBacklight(event); 
            }
            if (tmpString.equalsIgnoreCase(F("blOff"))){
              success = true;
              PCONFIG(0) = 0;
              setBacklight(event); 
            }
          break;
        }
        }
      }
  }  // switch (function)
  return success;
}

void setBacklight(struct EventStruct *event) {
  if (Settings.TaskDevicePin3[event->TaskIndex] != -1){
    pinMode(Settings.TaskDevicePin3[event->TaskIndex], OUTPUT);
    digitalWrite(Settings.TaskDevicePin3[event->TaskIndex], PCONFIG(0)); 
    portStatusStruct newStatus;
    const uint32_t   key = createKey(1, Settings.TaskDevicePin3[event->TaskIndex]);
    // WARNING: operator [] creates an entry in the map if key does not exist
    newStatus         = globalMapPortStatus[key];
    newStatus.command = 1;
    newStatus.mode    = PIN_MODE_OUTPUT;
    newStatus.state   = PCONFIG(3);
    savePortStatus(key, newStatus);
  }
}

boolean displayText(char deviceTemplate[][48], struct EventStruct *event ){ // 48 must be equal to "#define Digits_per_template_line"
//boolean displayText( char &deviceTemplate, struct EventStruct *event ){ // 48 must be equal to "#define Digits_per_template_line"
        String log = F("PCD8544: ");
        String string ;
        lcd3->clearDisplay();
        lcd3->setTextColor(BLACK);
        lcd3->setCursor(0,0);

        for (byte x = 0; x < lcd_lines; x++)
        {
          if (x <= 3){
            lcd3->setTextSize(PCONFIG(3+x)); 
          }else{
            lcd3->setTextSize(1);
          }
          String tmpString = deviceTemplate[x];
          String newString;
          if (tmpString.length()){
            newString = parseTemplate(tmpString, false);
          }else{
            // webformline is empty use html input
            newString = html_input[x]; 
            // 1e time html_input[x] has trailing spaces to delete old digits from the previous html_input[x] displayed on de LCD
            // Remove trailing spaces in html_input[x] for the next time
            while (newString.endsWith(" ")) {
              newString = newString.substring(0, newString.length() - 1);
            }
            int len = newString.length();
            if (len > digits_per_display_line) {len = digits_per_display_line;};  // max len is len html_input[x]
            newString = newString.substring(0,len);
            strncpy(html_input[x], newString.c_str(), len);
          }
          lcd3->println(newString);
          string+=newString+"\\";         
        }
        log += String(F("displayed text: \"")) + String(string) + String(F("\""));
        addLog(LOG_LEVEL_INFO, log);
        lcd3->display();
        return true;
  }

#endif // USES_P208
