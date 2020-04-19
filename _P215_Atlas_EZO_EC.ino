//########################################################################
//################## Plugin 214 : Atlas Scientific EZO EC sensor  ########
//########################################################################

// datasheet at https://www.atlas-scientific.com/_files/_datasheets/_circuit/EC_EZO_datasheet.pdf
// works only in i2c mode

#ifdef USES_P215

#define PLUGIN_215
#define PLUGIN_ID_215 215
#define PLUGIN_NAME_215       "Environment - Atlas Scientific EC EZO"
#define PLUGIN_VALUENAME1_215 "EC"

boolean Plugin_215_init = false;

boolean Plugin_215(byte function, struct EventStruct *event, String& string)
{
  boolean success = false;

  switch (function)
  {
    case PLUGIN_DEVICE_ADD:
      {
        Device[++deviceCount].Number = PLUGIN_ID_215;
        Device[deviceCount].Type = DEVICE_TYPE_I2C;
        Device[deviceCount].VType = SENSOR_TYPE_SINGLE;
        Device[deviceCount].Ports = 0;
        Device[deviceCount].PullUpOption = false;
        Device[deviceCount].InverseLogicOption = false;
        Device[deviceCount].FormulaOption = true;
        Device[deviceCount].ValueCount = 1;
        Device[deviceCount].SendDataOption = true;
        Device[deviceCount].TimerOption = true;
        Device[deviceCount].GlobalSyncOption = true;
        break;
      }

    case PLUGIN_GET_DEVICENAME:
      {
        string = F(PLUGIN_NAME_215);
        break;
      }

    case PLUGIN_GET_DEVICEVALUENAMES:
      {
        strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[0], PSTR(PLUGIN_VALUENAME1_215));
        break;
      }

    case PLUGIN_WEBFORM_LOAD:
      {
        #define ATLASEZO_EC_I2C_NB_OPTIONS 3
        byte I2Cchoice = Settings.TaskDevicePluginConfig[event->TaskIndex][0];
        int optionValues[ATLASEZO_EC_I2C_NB_OPTIONS] = { 0x64, 0x65, 0x66 };
        addFormSelectorI2C(F("Plugin_215_i2c"), ATLASEZO_EC_I2C_NB_OPTIONS, optionValues, I2Cchoice);

        addFormSubHeader(F("General"));

        char sensordata[32];
        bool status;
        status = _P215_send_I2C_command(Settings.TaskDevicePluginConfig[event->TaskIndex][0],"i",sensordata);

        if (status) {
          String boardInfo(sensordata);

          addHtml(F("<TR><TD>Board type : </TD><TD>"));
          int pos1 = boardInfo.indexOf(',');
          int pos2 = boardInfo.lastIndexOf(',');
          addHtml(boardInfo.substring(pos1+1,pos2));
          if (boardInfo.substring(pos1+1,pos2) != "EC"){
            addHtml(F("<span style='color:red'>  WARNING : Board type should be 'EC', check your i2c Address ? </span>"));
          }
          addHtml(F("</TD></TR><TR><TD>Board version :</TD><TD>"));
          addHtml(boardInfo.substring(pos2+1));
          addHtml(F("</TD></TR>\n"));

          addHtml(F("<input type='hidden' name='Plugin_215_sensorVersion' value='"));
          addHtml(boardInfo.substring(pos2+1));
          addHtml(F("'>"));

          status = _P215_send_I2C_command(Settings.TaskDevicePluginConfig[event->TaskIndex][0],"K,?",sensordata);
          boardInfo = sensordata;
          addHtml(F("<TR><TD>Sensor type : </TD><TD>K="));
          pos2 = boardInfo.lastIndexOf(',');
          addHtml(boardInfo.substring(pos2+1));
          addHtml(F("</TD></TR>\n"));

          status = _P215_send_I2C_command(Settings.TaskDevicePluginConfig[event->TaskIndex][0],"T,?",sensordata);
          if (status){
            boardInfo = sensordata;
            addHtml(F("<TR><TD>Temperature compensation : </TD><TD>T="));
            pos2 = boardInfo.lastIndexOf(',');
            addHtml(boardInfo.substring(pos2+1));
            addHtml(F("</TD></TR>\n"));
          }
        } else {
          addHtml(F("<span style='color:red;'>Unable to send command to device</span>"));
          success = false;
          break;
        }

        addFormCheckBox(F("Status LED"), F("Plugin_215_status_led"), Settings.TaskDevicePluginConfig[event->TaskIndex][1]);

        addFormSubHeader(F("Calibration"));
        addRowLabel(F("<strong>Clear calibration</strong>"));
        addFormCheckBox(F("Enable"),F("Plugin_215_clear_cal"), false);
        addHtml(F("\n<script type='text/javascript'>document.getElementById(\"Plugin_215_clear_cal\").onclick=function() {document.getElementById(\"Plugin_215_enable_cal_dry\").checked = false;document.getElementById(\"Plugin_215_enable_cal_single\").checked = false;document.getElementById(\"Plugin_215_enable_cal_L\").checked = false;;document.getElementById(\"Plugin_215_enable_cal_H\").checked = false;};</script>"));
        addFormNote(F("This will erase all calibration informations !"));

        addRowLabel(F("<strong>Dry calibration</strong>"));
        addFormCheckBox(F("Enable"),F("Plugin_215_enable_cal_dry"), false);
        addHtml(F("\n<script type='text/javascript'>document.getElementById(\"Plugin_215_enable_cal_dry\").onclick=function() {document.getElementById(\"Plugin_215_clear_cal\").checked = false;document.getElementById(\"Plugin_215_enable_cal_single\").checked = false;document.getElementById(\"Plugin_215_enable_cal_L\").checked = false;;document.getElementById(\"Plugin_215_enable_cal_H\").checked = false;};</script>"));
        addFormNote(F("Dry calibration must always be done first!"));

        addRowLabel(F("<strong>Single point calibration</strong> "));
        addFormCheckBox(F("Enable"),F("Plugin_215_enable_cal_single"), false);
        addHtml(F("\n<script type='text/javascript'>document.getElementById(\"Plugin_215_enable_cal_single\").onclick=function() {document.getElementById(\"Plugin_215_clear_cal\").checked = false;document.getElementById(\"Plugin_215_enable_cal_dry\").checked = false;document.getElementById(\"Plugin_215_enable_cal_L\").checked = false;;document.getElementById(\"Plugin_215_enable_cal_H\").checked = false;};</script>"));
        addFormNumericBox(F("Ref EC"),F("Plugin_215_ref_cal_single"), Settings.TaskDevicePluginConfigLong[event->TaskIndex][0],0,1000000);
        addUnit("&micro;S");

        addRowLabel(F("<strong>Two points calibration</strong> "));
        addRowLabel(F("Low calibration"));
        addFormCheckBox(F("enable"),F("Plugin_215_enable_cal_L' onClick='document.getElementById(\"Plugin_215_clear_cal\").checked = false;document.getElementById(\"Plugin_215_enable_cal_dry\").checked = false;document.getElementById(\"Plugin_215_enable_cal_single\").checked = false;;document.getElementById(\"Plugin_215_enable_cal_H\").checked = false;"), false);
        addHtml(F("\n<script type='text/javascript'>document.getElementById(\"Plugin_215_enable_cal_L\").onclick=function() {document.getElementById(\"Plugin_215_enable_cal_dry\").checked = false;document.getElementById(\"Plugin_215_enable_cal_single\").checked = false;;document.getElementById(\"Plugin_215_enable_cal_H\").checked = false;};</script>"));
        addFormNumericBox(F("Ref EC"),F("Plugin_215_ref_cal_L"), Settings.TaskDevicePluginConfigLong[event->TaskIndex][1],0,1000000);
        addUnit("&micro;S");

        addRowLabel(F("High calibration"));
        addFormCheckBox(F("Enable"),F("Plugin_215_enable_cal_H"), false);
        addHtml(F("\n<script type='text/javascript'>document.getElementById(\"Plugin_215_clear_cal\").checked = false;document.getElementById(\"Plugin_215_enable_cal_H\").onclick=function() {document.getElementById(\"Plugin_215_enable_cal_dry\").checked = false;document.getElementById(\"Plugin_215_enable_cal_single\").checked = false;;document.getElementById(\"Plugin_215_enable_cal_L\").checked = false;};</script>"));
        addFormNumericBox(F("Ref EC"),F("Plugin_215_ref_cal_H"), Settings.TaskDevicePluginConfigLong[event->TaskIndex][2],0,1000000);
        addUnit("&micro;S");

        status = _P215_send_I2C_command(Settings.TaskDevicePluginConfig[event->TaskIndex][0], "Cal,?",sensordata);
        if (status){
          switch (sensordata[5]) {
            case '0' :
              addFormNote(F("<span style='color:red'>Calibration needed</span>"));
            break;
            case '1' :
              addFormNote(F("<span style='color:green'>Single point calibration ok</span>"));
            break;
            case '2' :
              addFormNote(F("<span style='color:green'>Two points calibration ok</span>"));
            break;
          }
        }

        addFormSubHeader(F("Temperature compensation"));
        addFormCheckBox(F("Enable Temperature compensation"), F("Plugin_215_enable_temp_compensation"), Settings.TaskDevicePluginConfig[event->TaskIndex][2]);
        addFormNote(F("Should be disabled during calibration"));

        char deviceTemperatureTemplate[40];
        LoadCustomTaskSettings(event->TaskIndex, (byte*)&deviceTemperatureTemplate, sizeof(deviceTemperatureTemplate));
        addFormTextBox(F("Temperature "), F("Plugin_215_temperature_template"), deviceTemperatureTemplate, sizeof(deviceTemperatureTemplate));
        addFormNote(F("You can use a formulae (and idealy refer to a temp sensor). "));
        float value;
        char strValue[5];
        addHtml(F("<div class='note'>"));
        if (_P215_get_temperature(deviceTemperatureTemplate,&value) == CALCULATE_OK ){
          addHtml(F("Actual value : "));
          dtostrf(value,5,2,strValue);
          addHtml(strValue);
        } else {
          addHtml(F("(It seems I can't parse your formulae)"));
        }
        addHtml(F("</div>"));

        success = true;
        break;
      }

    case PLUGIN_WEBFORM_SAVE:
      {
        Settings.TaskDevicePluginConfig[event->TaskIndex][0] = getFormItemInt(F("Plugin_215_i2c"));

        Settings.TaskDevicePluginConfigFloat[event->TaskIndex][0] = getFormItemFloat(F("Plugin_215_sensorVersion"));

        char sensordata[32];
        if (isFormItemChecked(F("Plugin_215_status_led"))) {
          _P215_send_I2C_command(Settings.TaskDevicePluginConfig[event->TaskIndex][0],"L,1",sensordata);
        } else {
          _P215_send_I2C_command(Settings.TaskDevicePluginConfig[event->TaskIndex][0],"L,0",sensordata);
        }
        Settings.TaskDevicePluginConfig[event->TaskIndex][1] = isFormItemChecked(F("Plugin_215_status_led"));

        long ref_cal_single = getFormItemInt(F("Plugin_215_ref_cal_single"));
        long ref_cal_L = getFormItemInt(F("Plugin_215_ref_cal_L"));
        long ref_cal_H = getFormItemInt(F("Plugin_215_ref_cal_H"));
        Settings.TaskDevicePluginConfigLong[event->TaskIndex][0] = ref_cal_single;
        Settings.TaskDevicePluginConfigLong[event->TaskIndex][1] = ref_cal_L;
        Settings.TaskDevicePluginConfigLong[event->TaskIndex][2] = ref_cal_H;

        String cmd ("Cal,");
        bool triggerCalibrate = false;
        if (isFormItemChecked("Plugin_215_enable_cal_dry")) {
          cmd += "dry";
          triggerCalibrate = true;
        }
        else if (isFormItemChecked("Plugin_215_clear_cal")) {
          cmd += "clear";
          triggerCalibrate = true;
        } else if (isFormItemChecked("Plugin_215_enable_cal_single")){
          cmd += ref_cal_single;
          triggerCalibrate = true;
        } else if (isFormItemChecked("Plugin_215_enable_cal_L")){
          cmd += "low,";
          cmd += ref_cal_L;
          triggerCalibrate = true;
        } else if (isFormItemChecked("Plugin_215_enable_cal_H")){
          cmd += "high,";
          cmd += ref_cal_H;
          triggerCalibrate = true;
        }
        if (triggerCalibrate){
          char sensordata[32];
          _P215_send_I2C_command(Settings.TaskDevicePluginConfig[event->TaskIndex][0],cmd.c_str(),sensordata);
        }

        char deviceTemperatureTemplate[40];
        String tmpString = WebServer.arg(F("Plugin_215_temperature_template"));
        strncpy(deviceTemperatureTemplate, tmpString.c_str(), sizeof(deviceTemperatureTemplate)-1);
        deviceTemperatureTemplate[sizeof(deviceTemperatureTemplate)-1]=0; //be sure that our string ends with a \0

        SaveCustomTaskSettings(event->TaskIndex, (byte*)&deviceTemperatureTemplate, sizeof(deviceTemperatureTemplate));

        Settings.TaskDevicePluginConfig[event->TaskIndex][2] = isFormItemChecked(F("Plugin_215_enable_temp_compensation"));

        Plugin_215_init = false;
        success = true;
        break;
      }

    case PLUGIN_INIT:
      {
        Plugin_215_init = true;
      }

    case PLUGIN_READ:
      {
        char sensordata[32];
        bool status;

        //first set the temperature of reading (if temperature compensation it is enabled)

        if (Settings.TaskDevicePluginConfig[event->TaskIndex][2] != 0) {
          char deviceTemperatureTemplate[40];
          LoadCustomTaskSettings(event->TaskIndex, (byte*)&deviceTemperatureTemplate, sizeof(deviceTemperatureTemplate));

          String setTemperature("T,");
          float temperatureReading;
          if (_P215_get_temperature(deviceTemperatureTemplate,&temperatureReading) == CALCULATE_OK ){
            setTemperature += temperatureReading;
          } else {
            success = false;
            break;
          }

          status = _P215_send_I2C_command(Settings.TaskDevicePluginConfig[event->TaskIndex][0],setTemperature.c_str(),sensordata);
        }

        //ok, now we can read the EC value
        status = _P215_send_I2C_command(Settings.TaskDevicePluginConfig[event->TaskIndex][0],"r",sensordata);

        if (status){
          String sensorString(sensordata);
          UserVar[event->BaseVarIndex] = sensorString.toFloat();
        }
        else {
          UserVar[event->BaseVarIndex] = -1;
        }

        //go to sleep
        //status = _P215_send_I2C_command(Settings.TaskDevicePluginConfig[event->TaskIndex][0],"Sleep",sensordata);

        success = true;
        break;
      }
      case PLUGIN_WRITE:
        {
          //TODO : do something more usefull ...

          String tmpString  = string;
          int argIndex = tmpString.indexOf(',');
          if (argIndex)
            tmpString = tmpString.substring(0, argIndex);
          if (tmpString.equalsIgnoreCase(F("ATLASCMD")))
          {
            success = true;
            argIndex = string.lastIndexOf(',');
            tmpString = string.substring(argIndex + 1);
            if (tmpString.equalsIgnoreCase(F("CalMid"))){
              String log("Asking for Mid calibration ");
              addLog(LOG_LEVEL_INFO, log);
            }
            else if (tmpString.equalsIgnoreCase(F("CalLow"))){
              String log("Asking for Low calibration ");
              addLog(LOG_LEVEL_INFO, log);
            }
            else if (tmpString.equalsIgnoreCase(F("CalHigh"))){
              String log("Asking for High calibration ");
              addLog(LOG_LEVEL_INFO, log);
            }
          }
          break;
        }
  }
  return success;
}

// Call this function with two char arrays, one containing the command
// The other containing an allocatted char array for answer
// Returns true on success, false otherwise

bool _P215_send_I2C_command(uint8_t I2Caddress,const char * cmd, char* sensordata) {
    uint16_t sensor_bytes_received = 0;

    byte error;
    byte i2c_response_code = 0;
    byte in_char = 0;

    Serial.print( F("EZO EC <"));
    Serial.println(cmd);
    Wire.beginTransmission(I2Caddress);
    Wire.write(cmd);
    error = Wire.endTransmission();

    if (error != 0) {
      return false;
    }

    //don't read answer if we want to go to sleep
    if (strncmp(cmd,"Sleep",5) == 0) {
      return true;
    }

    i2c_response_code = 254;
    while (i2c_response_code == 254) {      // in case the cammand takes longer to process, we keep looping here until we get a success or an error

      if  (
             (  (cmd[0] == 'r' || cmd[0] == 'R') && cmd[1] == '\0'  )
             ||
             (  ( strncmp(cmd,"cal",3) || strncmp(cmd,"Cal",3) ) && !strncmp(cmd,"Cal,?",5) )
           )
      {
        delay(600);
      }
      else {
        delay(300);
      }

      Wire.requestFrom(I2Caddress, (uint8_t) 32);    //call the circuit and request 32 bytes (this is more then we need).
      i2c_response_code = Wire.read();      //read response code

      while (Wire.available()) {            //read response
        in_char = Wire.read();

        if (in_char == 0) {                 //if we receive a null caracter, we're done
          while (Wire.available()) {  //purge the data line if needed
            Wire.read();
          }

          break;                            //exit the while loop.
        }
        else {
          sensordata[sensor_bytes_received] = in_char;        //load this byte into our array.
          sensor_bytes_received++;
        }
      }
      sensordata[sensor_bytes_received] = '\0';

      switch (i2c_response_code) {
        case 1:
          Serial.print( F("EZO EC > success, answer = "));
          Serial.println(sensordata);
          break;

        case 2:
          Serial.println( F("EZO EC > command failed"));
          return false;

        case 254:
          Serial.println( F("EZO EC > command pending"));
          break;

        case 255:
          Serial.println( F("EZO EC > no data"));
          return false;
      }
    }

    return true;
}

int _P215_get_temperature(char * formula,float * result) {
  String formulaTemplate;
  String parsedFormula;

  formulaTemplate = formula;
  parsedFormula = parseTemplate(formulaTemplate,formulaTemplate.length());

  return Calculate(parsedFormula.c_str(),result);
}

#endif
