//#define USES_P172
#ifdef USES_P172
//#######################################################################################################
//################### Plugin 172 PZEM-004Tv3 AC Current and Voltage measurement sensor ##################
//#######################################################################################################
/*
    Needed modifications in ESPEasy source code
        In _CPlugin_SensorTypeHelper.ino:
        Insert at line 30 and 31
                case SENSOR_TYPE_SEX:
                    return 6;

        In ESPEasy_checks.ino:
        Change line 28:
        check_size<ExtraTaskSettingsStruct,               472u>();
        to:
        check_size<ExtraTaskSettingsStruct,               640u>();

        In DeviceStruct.h:
        Insert at line 30:
        #define SENSOR_TYPE_SEX                    66 

        In Custom.h:
        Add line:
        #define VARS_PER_TASK 6
        and:
        #define USES_P172   // PZEM-004T version 3
        And your own configuration settings ofcourse

*/
//#ifdef PLUGIN_BUILD_TESTING

#include <ESPeasySerial.h>
#include <vector>

#define PLUGIN_172
#define PLUGIN_ID_172           172

#define PLUGIN_NAME_172         "Energy (AC) PZEM-004T version 3"
#define PLUGIN_VALUENAME1_172   "Voltage"
#define PLUGIN_VALUENAME2_172   "Current"
#define PLUGIN_VALUENAME3_172   "Power"
#define PLUGIN_VALUENAME4_172   "Energy"
#define PLUGIN_VALUENAME5_172   "Frequency"
#define PLUGIN_VALUENAME6_172   "PowerFactor"

#define P172_REG_VOLTAGE        0x0000
#define P172_REG_CURRENT_L      0x0001
#define P172_REG_CURRENT_H      0X0002
#define P172_REG_POWER_L        0x0003
#define P172_REG_POWER_H        0x0004
#define P172_REG_ENERGY_L       0x0005
#define P172_REG_ENERGY_H       0x0006
#define P172_REG_FREQUENCY      0x0007
#define P172_REG_PF             0x0008
#define P172_REG_ALARM          0x0009

#define P172_CMD_RHR            0x03
#define P172_CMD_RIR            0X04
#define P172_CMD_WSR            0x06
#define P172_CMD_CAL            0x41
#define P172_CMD_REST           0x42


#define P172_WREG_ALARM_THR     0x0001
#define P172_WREG_ADDR          0x0002

#define P172_UPDATE_TIME        200

#define P172_RESPONSE_SIZE      32
#define P172_READ_TIMEOUT       200
#define P172_DELAY_TIMEOUT      2000
#define P172_ERROR_VALUE        -1
#define P172_PZEM_BAUD_RATE     9600


ESPeasySerial *P172_easySerial = nullptr;
static bool P172_first_init_done = false;
static int P172_rx_pin = 0;
static int P172_tx_pin = 0;

static uint8_t P172_first_unit_index = 255;
uint64_t P172_lastRead; 
unsigned long P172_timeout_msec;

struct P172_PZEM004T_values
{
    taskIndex_t task_index;
    float voltage;
    float current;
    float power;
    float energy;
    float frequency;
    float pf;
    uint16_t alarms;
    uint8_t slave_addr_check;
    uint8_t slave_addr;
};  // Measured values
static std::vector<P172_PZEM004T_values> P172_PZEM004T_values_vector;

void P172_ResetPort()
{
    if (P172_easySerial != nullptr) {
        delete P172_easySerial;
        P172_easySerial = nullptr;
    }
}

/*
* sendCmd8
*
* Prepares the 8 byte command buffer and sends
*
* @param[in] cmd - Command to send (position 1)
* @param[in] rAddr - Register address (postion 2-3)
* @param[in] val - Register value to write (positon 4-5)
* @param[in] check - perform a simple read check after write
* @param[in] slave_addr - device address
*
* @return success
*/
bool P172_sendCmd8(uint8_t cmd, uint16_t rAddr, uint16_t val, bool check, uint8_t slave_addr)
{
    uint8_t sendBuffer[8]; // Send buffer
    uint8_t respBuffer[8]; // Response buffer (only used when check is true)

    if ((slave_addr < 0x01) || (slave_addr > 0xF8))
    {
        /* Address out of range */
        String log = F("PZEM004Tv3: Address out of range. Addr is ");
        log += String(slave_addr, 16);
        addLog(LOG_LEVEL_DEBUG, log);
        return false;
    }
    sendBuffer[0] = slave_addr; // Set slave address
    sendBuffer[1] = cmd;        // Set command

    sendBuffer[2] = (rAddr >> 8) & 0xFF; // Set high byte of register address
    sendBuffer[3] = (rAddr)&0xFF;        // Set low byte =//=

    sendBuffer[4] = (val >> 8) & 0xFF; // Set high byte of register value
    sendBuffer[5] = (val)&0xFF;        // Set low byte =//=

    P172_setCRC(sendBuffer, 8); // Set CRC of frame

    P172_easySerial->write(sendBuffer, 8); // send frame

    if (check)
    {
        if (!P172_recieve(respBuffer, 8))
        { // if check enabled, read the response
            return false;
        }

        // Check if response is same as send
        for (uint8_t i = 0; i < 8; i++)
        {
            if (sendBuffer[i] != respBuffer[i])
            {
                return false;
            }
        }
    }
    return true;
}

/*!
* setAddress
*
* Set a new device address and update the device
* WARNING - should be used to set up devices once.
*
* @param[in] addr New device address 0x01-0xF7
*
* @return success
*/
bool P172_setAddress(uint8_t addr)
{
    if(addr < 0x00 || addr > 0xF7) // sanity check
    {
        return false;
    }
    // Write the new address to the address register
    if(!P172_sendCmd8(P172_CMD_WSR, P172_WREG_ADDR, addr, true, 0xF8))
    {
        return false;
    }
        
    return true;
}

bool P172_setPowerAlarm(uint16_t watts, uint8_t slave_addr)
{
    if (watts > 25000){ // Sanitych check
        watts = 25000;
    }

    // Write the watts threshold to the Alarm register
    if(!P172_sendCmd8(P172_CMD_WSR, P172_WREG_ALARM_THR, watts, true, slave_addr))
    {
        return false;
    }
    return true;
}
/*
bool P172_getPowerAlarm(uint8_t slave_addr)
{
    if(!updateValues(slave_addr)) // Update vales if necessary
        return P172_ERROR_VALUE;

    return _currentValues.alarms != 0x0000;
}
*/

bool P172_updateValues(uint8_t vector_index)
{
    static uint8_t response[25];
    uint8_t slave_addr = P172_PZEM004T_values_vector[vector_index].slave_addr;
    if(loglevelActiveFor(LOG_LEVEL_DEBUG_DEV))
    {
        String log = F("PZEM004Tv3: requested data from ");
        log += String(slave_addr);
        addLog(LOG_LEVEL_DEBUG_DEV, log);
    }
    
    // Read 10 registers starting at 0x00 (no check)
    if(!P172_sendCmd8(P172_CMD_RIR, 0x00, 0x0A, false, slave_addr))
    {
        addLog(LOG_LEVEL_DEBUG, F("PZEM004Tv3: sendCmd8 failed!"));
        P172_easySerial->flush();
        return false;
    }
    
    if(P172_recieve(response, 25) != 25) // Something went wrong
    {
        String log = F("PZEM004Tv3: receive failed. Got ");
        for(int i = 0; i<26; i++)
        {
            log+=String(response[i]);
        }
        log += ".";
        addLog(LOG_LEVEL_DEBUG, log);
        P172_easySerial->flush();
        return false;
    }
    
    P172_PZEM004T_values_vector[vector_index].slave_addr_check = (uint8_t)response[0];    // Slave address
    if(P172_PZEM004T_values_vector[vector_index].slave_addr_check != slave_addr)
    {
        //We received data from the wrong device
        String log = F("PZEM004Tv3: requested data from ");
        log += String(slave_addr);
        log += F(" but got a response from ");
        log += String(P172_PZEM004T_values_vector[vector_index].slave_addr_check);
        log += F(" !!");
        addLog(LOG_LEVEL_INFO, log);
        return false;
    }

    P172_PZEM004T_values_vector[vector_index].voltage =  ((uint32_t)response[3] << 8 | // Raw voltage in 0.1V
                                                        (uint32_t)response[4])/10.0;

    P172_PZEM004T_values_vector[vector_index].current =  ((uint32_t)response[5] << 8 | // Raw current in 0.001A
                                                        (uint32_t)response[6] |
                                                        (uint32_t)response[7] << 24 |
                                                        (uint32_t)response[8] << 16) / 1000.0;

    P172_PZEM004T_values_vector[vector_index].power =    ((uint32_t)response[9] << 8 | // Raw power in 0.1W
                                                        (uint32_t)response[10] |
                                                        (uint32_t)response[11] << 24 |
                                                        (uint32_t)response[12] << 16) / 10.0;

    P172_PZEM004T_values_vector[vector_index].energy =   ((uint32_t)response[13] << 8 | // Raw Energy in 1Wh
                                                        (uint32_t)response[14] |
                                                        (uint32_t)response[15] << 24 |
                                                        (uint32_t)response[16] << 16) / 1000.0;

    P172_PZEM004T_values_vector[vector_index].frequency =((uint32_t)response[17] << 8 | // Raw Frequency in 0.1Hz
                                                        (uint32_t)response[18]) / 10.0;

    P172_PZEM004T_values_vector[vector_index].pf =       ((uint32_t)response[19] << 8 | // Raw pf in 0.01
                                                        (uint32_t)response[20])/100.0;

    P172_PZEM004T_values_vector[vector_index].alarms =   ((uint32_t)response[21] << 8 | // Raw alarm value
                                                        (uint32_t)response[22]);
    
    return true;
}

/*!
* resetEnergy
*
* Reset the Energy counter on the device
* @param[in] slave_addr - device address
* 
* @return success
*/
bool P172_resetEnergy(uint8_t slave_addr)
{
    uint8_t buffer[] = {0x00, P172_CMD_REST, 0x00, 0x00};
    uint8_t reply[5];
    buffer[0] = slave_addr;

    P172_setCRC(buffer, 4);
    P172_easySerial->write(buffer, 4);

    uint16_t length = P172_recieve(reply, 5);

    if (length == 0 || length == 5)
    {
        return false;
    }

    return true;
}

/*!
* recieve
*
* Receive data from serial with buffer limit and timeout
*
* @param[out] resp Memory buffer to hold response. Must be at least `len` long
* @param[in] len Max number of bytes to read
*
* @return number of bytes read
*/
uint16_t P172_recieve(uint8_t *resp, uint16_t len)
{
    P172_easySerial->listen();

    unsigned long startTime = millis(); // Start time for Timeout
    uint8_t index = 0;                  // Bytes we have read
    while ((index < len) && (millis() - startTime < P172_timeout_msec))
    {
        if (P172_easySerial->available() > 0)
        {
            uint8_t c = (uint8_t)P172_easySerial->read();
            resp[index++] = c;
        }
        yield(); // do background netw tasks while blocked for IO (prevents ESP watchdog trigger)
    }

    // Check CRC with the number of bytes read
    if (!P172_checkCRC(resp, index))
    {
        return 0;
    }

    return index;
}

/*!
* checkCRC
*
* Performs CRC check of the buffer up to len-2 and compares check sum to last two bytes
*
* @param[in] data Memory buffer containing the frame to check
* @param[in] len  Length of the respBuffer including 2 bytes for CRC
*
* @return is the buffer check sum valid
*/
bool P172_checkCRC(const uint8_t *buf, uint16_t len)
{
    if (len <= 2) // Sanity check
        return false;

    uint16_t crc = P172_CRC16(buf, len - 2); // Compute CRC of data
    return ((uint16_t)buf[len - 2] | (uint16_t)buf[len - 1] << 8) == crc;
}

/*!
* setCRC
*
* Set last two bytes of buffer to CRC16 of the buffer up to byte len-2
* Buffer must be able to hold at least 3 bytes
*
* @param[out] data Memory buffer containing the frame to checksum and write CRC to
* @param[in] len  Length of the respBuffer including 2 bytes for CRC
*
*/
void P172_setCRC(uint8_t *buf, uint16_t len)
{
    if (len <= 2) // Sanity check
        return;

    uint16_t crc = P172_CRC16(buf, len - 2); // CRC of data

    // Write high and low byte to last two positions
    buf[len - 2] = crc & 0xFF;        // Low byte first
    buf[len - 1] = (crc >> 8) & 0xFF; // High byte second
}

// Pre computed CRC table
const uint16_t P172_crcTable[256] = {
    0X0000, 0XC0C1, 0XC181, 0X0140, 0XC301, 0X03C0, 0X0280, 0XC241,
    0XC601, 0X06C0, 0X0780, 0XC741, 0X0500, 0XC5C1, 0XC481, 0X0440,
    0XCC01, 0X0CC0, 0X0D80, 0XCD41, 0X0F00, 0XCFC1, 0XCE81, 0X0E40,
    0X0A00, 0XCAC1, 0XCB81, 0X0B40, 0XC901, 0X09C0, 0X0880, 0XC841,
    0XD801, 0X18C0, 0X1980, 0XD941, 0X1B00, 0XDBC1, 0XDA81, 0X1A40,
    0X1E00, 0XDEC1, 0XDF81, 0X1F40, 0XDD01, 0X1DC0, 0X1C80, 0XDC41,
    0X1400, 0XD4C1, 0XD581, 0X1540, 0XD701, 0X17C0, 0X1680, 0XD641,
    0XD201, 0X12C0, 0X1380, 0XD341, 0X1100, 0XD1C1, 0XD081, 0X1040,
    0XF001, 0X30C0, 0X3180, 0XF141, 0X3300, 0XF3C1, 0XF281, 0X3240,
    0X3600, 0XF6C1, 0XF781, 0X3740, 0XF501, 0X35C0, 0X3480, 0XF441,
    0X3C00, 0XFCC1, 0XFD81, 0X3D40, 0XFF01, 0X3FC0, 0X3E80, 0XFE41,
    0XFA01, 0X3AC0, 0X3B80, 0XFB41, 0X3900, 0XF9C1, 0XF881, 0X3840,
    0X2800, 0XE8C1, 0XE981, 0X2940, 0XEB01, 0X2BC0, 0X2A80, 0XEA41,
    0XEE01, 0X2EC0, 0X2F80, 0XEF41, 0X2D00, 0XEDC1, 0XEC81, 0X2C40,
    0XE401, 0X24C0, 0X2580, 0XE541, 0X2700, 0XE7C1, 0XE681, 0X2640,
    0X2200, 0XE2C1, 0XE381, 0X2340, 0XE101, 0X21C0, 0X2080, 0XE041,
    0XA001, 0X60C0, 0X6180, 0XA141, 0X6300, 0XA3C1, 0XA281, 0X6240,
    0X6600, 0XA6C1, 0XA781, 0X6740, 0XA501, 0X65C0, 0X6480, 0XA441,
    0X6C00, 0XACC1, 0XAD81, 0X6D40, 0XAF01, 0X6FC0, 0X6E80, 0XAE41,
    0XAA01, 0X6AC0, 0X6B80, 0XAB41, 0X6900, 0XA9C1, 0XA881, 0X6840,
    0X7800, 0XB8C1, 0XB981, 0X7940, 0XBB01, 0X7BC0, 0X7A80, 0XBA41,
    0XBE01, 0X7EC0, 0X7F80, 0XBF41, 0X7D00, 0XBDC1, 0XBC81, 0X7C40,
    0XB401, 0X74C0, 0X7580, 0XB541, 0X7700, 0XB7C1, 0XB681, 0X7640,
    0X7200, 0XB2C1, 0XB381, 0X7340, 0XB101, 0X71C0, 0X7080, 0XB041,
    0X5000, 0X90C1, 0X9181, 0X5140, 0X9301, 0X53C0, 0X5280, 0X9241,
    0X9601, 0X56C0, 0X5780, 0X9741, 0X5500, 0X95C1, 0X9481, 0X5440,
    0X9C01, 0X5CC0, 0X5D80, 0X9D41, 0X5F00, 0X9FC1, 0X9E81, 0X5E40,
    0X5A00, 0X9AC1, 0X9B81, 0X5B40, 0X9901, 0X59C0, 0X5880, 0X9841,
    0X8801, 0X48C0, 0X4980, 0X8941, 0X4B00, 0X8BC1, 0X8A81, 0X4A40,
    0X4E00, 0X8EC1, 0X8F81, 0X4F40, 0X8D01, 0X4DC0, 0X4C80, 0X8C41,
    0X4400, 0X84C1, 0X8581, 0X4540, 0X8701, 0X47C0, 0X4680, 0X8641,
    0X8201, 0X42C0, 0X4380, 0X8341, 0X4100, 0X81C1, 0X8081, 0X4040};

/*!
* CRC16
*
* Calculate the CRC16-Modbus for a buffer
* Based on https://www.modbustools.com/modbus_crc16.html
*
* @param[in] data Memory buffer containing the data to checksum
* @param[in] len  Length of the respBuffer
*
* @return Calculated CRC
*/
uint16_t P172_CRC16(const uint8_t *data, uint16_t len)
{
    uint8_t nTemp;         // CRC table index
    uint16_t crc = 0xFFFF; // Default value

    while (len--)
    {
        nTemp = *data++ ^ crc;
        crc >>= 8;
        crc ^= (uint16_t)pgm_read_word(&P172_crcTable[nTemp]);
    }
    return crc;
}
// End of PZEM-004Tv30m
bool P172_Check_if_task_exist(taskIndex_t taskindex)  //And delete it if exist
{
    if(P172_PZEM004T_values_vector.size() < 1)  //Only 1 or no items
    {
        return false;
    }
    bool did_exist = false;
    static std::vector<P172_PZEM004T_values> newVector;
    newVector.clear();
    for (uint16_t i = 0; i < P172_PZEM004T_values_vector.size(); i++)
    {
        if(P172_PZEM004T_values_vector[i].task_index == taskindex)
        {
            addLog(LOG_LEVEL_DEBUG, F("PZEM004Tv3: Double task found, removing.."));
            did_exist = false;
        }
        else
        {
            newVector.push_back(P172_PZEM004T_values_vector[i]);
        }
    }

    newVector.swap(P172_PZEM004T_values_vector);
    newVector.clear();
    return did_exist;
}


boolean Plugin_172(byte function, struct EventStruct *event, String& string)
{
  boolean success = false;

  switch (function)
  {
    case PLUGIN_DEVICE_ADD:
    {
        Device[++deviceCount].Number = PLUGIN_ID_172;
        Device[deviceCount].Type = DEVICE_TYPE_DUMMY;
        Device[deviceCount].VType = SENSOR_TYPE_SEX;
        Device[deviceCount].Ports = 0;
        Device[deviceCount].PullUpOption = false;
        Device[deviceCount].InverseLogicOption = false;
        Device[deviceCount].FormulaOption = true;
        Device[deviceCount].ValueCount = 6;
        Device[deviceCount].SendDataOption = false;  //we do it with this plugin
        Device[deviceCount].TimerOption = true;
        Device[deviceCount].DecimalsOnly = false;
        break;
    }

    case PLUGIN_GET_DEVICENAME:
    {
        string = F(PLUGIN_NAME_172);
        break;
    }

    case PLUGIN_GET_DEVICEVALUENAMES:
    {
        strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[0], PSTR(PLUGIN_VALUENAME1_172));
        strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[1], PSTR(PLUGIN_VALUENAME2_172));
        strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[2], PSTR(PLUGIN_VALUENAME3_172));
        strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[3], PSTR(PLUGIN_VALUENAME4_172));
        strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[4], PSTR(PLUGIN_VALUENAME5_172));
        strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[5], PSTR(PLUGIN_VALUENAME6_172));
        break;
    }

    case PLUGIN_GET_DEVICEGPIONAMES:
    {
        
        break;
    }

    case PLUGIN_WEBFORM_LOAD:
    {
        uint8_t slave_addr = (uint8_t)PCONFIG(0);
        
        if((P172_first_init_done) && (event->TaskIndex != P172_first_unit_index))
        {
            addHtml(F("<br><span style=\"color:darkgreen;\">Using serialport settings from first PZEM004Tv3 device</span>"));
        }
        else
        {
            addHtml(F("<TR><TD>ESP RX pin:<TD>"));
            addPinSelect(false, F("taskdevicepin1"), CONFIG_PIN1);
            addHtml(F("<TR><TD>ESP TX pin:<TD>"));
            addPinSelect(false, F("taskdevicepin2"), CONFIG_PIN2);
            
            //addFormNote(F("SoftSerial: 1st=RX-Pin 14 2nd=TX-Pin 12"));
        }
        
        addFormNumericBox(F("Device Address"), F("p172_slave_addr"), slave_addr, 0, 248);
        
        addHtml(F("</tr><tr><td></td><td \"height:20px;\"><p style=\"font-size:10pt\">Enter device address in decimal (0~248). <span style=\"color:#ff0000;\">Use 0 only when using just one device.</span></p></td></tr>"));
        
        addHtml(F("<tr><td></td><td \"height:20px;\"><p style=\"font-size:10pt;\">To use multiple devices on one UART port, wire them like <a style=\"font-size:10pt;\" href=\"https://github.com/Jansemar/PZEM004Tv30MULTI/raw/master/multiuartdevs.png\" target=\"_blank\" rel=\"noopener\">this</a></p></td></tr>"));
        addHtml(F("<tr><td></td><td \"height:20px;\"><p style=\"font-size:10pt;\"><span style=\"color:#ff0000;\">WARNING </span>The PZEM outputs 5V and the ESP can handle only 3.3V Use a levelshifter, </p></td></tr>"));
        addHtml(F("<tr><td></td><td \"height:20px;\"><p style=\"font-size:10pt;\">voltage divider or mod your PZEM to prevent damage to your ESP.</p></td></tr>"));
        addFormSubHeader(F("How to assign an address and clear energy value"));
        addHtml(F("<tr><td colspan=\"2; height:20px;\"><p style=\"font-size:10pt;\">To set a device address, connect the device you want to use to your ESP using the pins you configured at the first pzem device.</p></td></tr>"));
        addHtml(F("<tr><td colspan=\"2; height:20px;\"><p style=\"font-size:10pt;\">Go to the TOOLS tab on your ESPEasy and enter the command: <strong><em>pzemsetaddr,X</em></strong> where X is the address u want to set.</p></td></tr>"));
        addHtml(F("<tr><td colspan=\"2; height:20px;\"><p style=\"font-size:10pt;\">To reset the total power value enter the command: <strong><em>resetpzemenergy,X</em></strong> where X is the device address u want to reset.</p></td></tr>"));
        addHtml(F("<tr><td colspan=\"2; height:20px;\"><p style=\"font-size:10pt;\">If you enter <strong><em>resetpzemenergy</em></strong> without an address the power values from all devices will be set to 0.</p></td></tr>"));
        addHtml(F("<tr><td colspan=\"2; height:20px;\"><p style=\"font-size:10pt;\">All commands can also be executed the usual ESPEasy way trough web or MQTT. See ESPEasy docs for more information.</p></td></tr>"));
        
        addFormSubHeader(F("Data Formatting"));
        addFormCheckBox(F("Send data in json format, instead of a single value on 6 seperate topics?"), F("p172_sendjson"), PCONFIG(1));

        addFormSubHeader(F("Data Acquisition"));
         
        addFormCheckBox("Send to Controller", F("p172_send2controller"), PCONFIG(2));
        
        html_TR_TD();
        addHtml(getControllerSymbol(0));  //Only send to first controller is supported in this plugin
        html_TD(1);
        addFormNote(F("Only sending to first controller is supported"));
        success = true;
        break;
    }

    case PLUGIN_WEBFORM_SAVE:
    {
        PCONFIG(0) = getFormItemInt(F("p172_slave_addr"));
        PCONFIG(1) = isFormItemChecked(F("p172_sendjson"));
        PCONFIG(2) = isFormItemChecked(F("p172_send2controller"));
        if( (!PCONFIG(1)) && (PCONFIG(2)) )
        {
            Settings.TaskDeviceSendData[0][event->TaskIndex] = true; // Send values the usual way
        }
        else
        {
            Settings.TaskDeviceSendData[0][event->TaskIndex] = false;
        }
        
        P172_Check_if_task_exist(event->TaskIndex);

        success = true;
        break;
    }

    case PLUGIN_WEBFORM_SHOW_CONFIG:
    {
        
        success = true;
        break;
    }

    case PLUGIN_READ:
    {
        //uint8_t slave_addr = (uint8_t)PCONFIG(0);
        bool sendInJson = PCONFIG(1);
        bool send2Controller = PCONFIG(2);
        taskIndex_t currentIndex = 255;
        for (int i = 0; i < (int)P172_PZEM004T_values_vector.size(); i++)
        {
            if(P172_PZEM004T_values_vector[i].task_index == event->TaskIndex)
            {
                currentIndex = P172_PZEM004T_values_vector[i].task_index;
            }
        }
        if(currentIndex == 255)
        {
            addLog(LOG_LEVEL_INFO, F("PZEM004Tv3: Index not found, values not updated!"));
            success = false;
            break;
        }
    	//-------------------------------------------------------------------
        // readings can be ZERO if there's no AC input on the module.
        // in this case V A and W are reported correctly as ZERO but
        // the accumulated Energy paramenter will not be saved so to
        // preserve previous value
        //-------------------------------------------------------------------
        if (sendInJson && send2Controller)
        {
            MakeControllerSettings(ControllerSettings);
            LoadControllerSettings(event->ControllerIndex, ControllerSettings);
            if (ControllerSettings.checkHostReachable(true)) //Check connection first
            {
                String json_topic = ControllerSettings.Publish;
                parseControllerVariables(json_topic, event, false);
                json_topic.replace("/%valname%", "");
            
                String values_in_json = "{";
                values_in_json += to_json_object_value(ExtraTaskSettings.TaskDeviceValueNames[0],
                                                       String(P172_PZEM004T_values_vector[currentIndex].voltage)) +
                                  ",";
                values_in_json += to_json_object_value(ExtraTaskSettings.TaskDeviceValueNames[1],
                                                       String(P172_PZEM004T_values_vector[currentIndex].current)) +
                                  ",";
                values_in_json += to_json_object_value(ExtraTaskSettings.TaskDeviceValueNames[2],
                                                       String(P172_PZEM004T_values_vector[currentIndex].power)) +
                                  ",";
                if (P172_PZEM004T_values_vector[currentIndex].energy >= 0)
                {
                    values_in_json += to_json_object_value(ExtraTaskSettings.TaskDeviceValueNames[3],
                                                           String(P172_PZEM004T_values_vector[currentIndex].energy)) +
                                  ",";
                }
                values_in_json += to_json_object_value(ExtraTaskSettings.TaskDeviceValueNames[4],
                                                       String(P172_PZEM004T_values_vector[currentIndex].frequency)) +
                                  ",";
                values_in_json += to_json_object_value(ExtraTaskSettings.TaskDeviceValueNames[5],
                                                   String(P172_PZEM004T_values_vector[currentIndex].pf)) +
                              "}";
                addLog(LOG_LEVEL_DEBUG, values_in_json);

                MQTTpublish(event->ControllerIndex, json_topic.c_str(), values_in_json.c_str(), Settings.MQTTRetainFlag);
            }
            else
            {
                addLog(LOG_LEVEL_DEBUG_MORE, F("PZEM004Tv3: checkHostReachable = false"));
            }
            
        }

        UserVar[event->BaseVarIndex]     = P172_PZEM004T_values_vector[currentIndex].voltage;
        UserVar[event->BaseVarIndex + 1] = P172_PZEM004T_values_vector[currentIndex].current;
        UserVar[event->BaseVarIndex + 2] = P172_PZEM004T_values_vector[currentIndex].power;
        if (P172_PZEM004T_values_vector[currentIndex].energy>=0)  
        {
            UserVar[event->BaseVarIndex + 3] = P172_PZEM004T_values_vector[currentIndex].energy;
        }
        UserVar[event->BaseVarIndex + 4] = P172_PZEM004T_values_vector[currentIndex].frequency;
        UserVar[event->BaseVarIndex + 5] = P172_PZEM004T_values_vector[currentIndex].pf;
        
        success = true;
        break;
    }

    case PLUGIN_INIT:
    {
        addLog(LOG_LEVEL_DEBUG, F("-----------==========Plugin 172 init==============------------"));
        
        uint8_t slave_addr = (uint8_t)PCONFIG(0); // Easier to read code this way
        
        /* Check if we are the first device */
        if((!P172_first_init_done) || (event->TaskIndex == P172_first_unit_index))
        {
            P172_rx_pin = CONFIG_PIN1;
            P172_tx_pin = CONFIG_PIN2;
            P172_first_unit_index = event->TaskIndex;
            P172_first_init_done = true;
        }
        
        P172_PZEM004T_values newDevice = {event->TaskIndex, -1, -1, -1, -1, -1, -1, 0, 0, slave_addr};
        P172_PZEM004T_values_vector.push_back(newDevice);
        
        P172_ResetPort();
        
        P172_easySerial = new ESPeasySerial(P172_rx_pin, P172_tx_pin);
        P172_easySerial->begin(P172_PZEM_BAUD_RATE);
        P172_timeout_msec = P172_READ_TIMEOUT;

        if(loglevelActiveFor(LOG_LEVEL_DEBUG))
        {
            String log = F("PZEM004Tv3: Init done. Using ");
            log += P172_easySerial->getLogString();
            addLog(LOG_LEVEL_DEBUG, log);
        }
        
        success = true;
        break;
    }

    case PLUGIN_ONCE_A_SECOND:
    {
        if(event->TaskIndex == P172_first_unit_index) //only execute task if first device
        {
            for (uint8_t i = 0; i < P172_PZEM004T_values_vector.size(); i++)
            {
                P172_updateValues(i);
                yield();
            }
        }
        success = true;
    }
    
    case PLUGIN_EXIT:
	{
	
	    break;

	}

    case PLUGIN_WRITE:
    {
        //this case defines code to be executed when the plugin executes an action (command).
        //Commands can be accessed via rules or via http.
        //As an example, http://192.168.1.12//control?cmd=dothis
        //implies that there exists the comamnd "dothis"
        if(string.startsWith(F("setpzemaddress")) && !P172_first_init_done)
        {
            addLog(LOG_LEVEL_ERROR, F("PZEM004Tv3: Configure first device first before changing the address."));
            break;
        }

        if (!P172_first_init_done)
        {
            break;
        }

        String command = parseString(string, 1);
        if(command == F("setpzemaddress"))
        {
            String secondVal = parseString(string, 2);
            unsigned int slaveAddr = 0;
            if(validUIntFromString(secondVal, slaveAddr))
            {
                if((slaveAddr < 248) && (slaveAddr > 0))
                {
                    if(P172_setAddress((uint8_t)slaveAddr))
                    {
                        String log = F("PZEM004Tv3: Setting device address is successful. New address is ");
                        log += String(slaveAddr);
                        log += F(". (");
                        log += String(slaveAddr, 16);
                        log += F(")");
                        addLog(LOG_LEVEL_ERROR, log);
                        success = true;
                        break;
                    }
                    else
                    {
                        addLog(LOG_LEVEL_ERROR, F("PZEM004Tv3: Set address failed. Check your command and try again."));
                        break;
                    }
                    
                }
                else
                {
                    addLog(LOG_LEVEL_ERROR, F("PZEM004Tv3: Invalid device address. Must be between 0 and 248 (0xF8) "));
                    break;
                }
            }
            else
            {
                addLog(LOG_LEVEL_ERROR, F("PZEM004Tv3: Invalid device address. It's not a positive number!"));
                break;
            }
        }
        else if(command == F("resetpzemenergy"))
        {
            String secondVal = parseString(string, 2);
            unsigned int slaveAddr = 0;
            if(validUIntFromString(secondVal, slaveAddr))
            {
                if((slaveAddr < 248) && (slaveAddr > 0))
                {
                    if(P172_resetEnergy((uint8_t)slaveAddr))
                    {
                        String log = F("PZEM004Tv3: Resetting device energy is successful. Used device address is ");
                        log += String(slaveAddr);
                        log += F(". (");
                        log += String(slaveAddr, 16);
                        log += F(")");
                        addLog(LOG_LEVEL_INFO, log);
                        success = true;
                        break;
                    }
                    else
                    {
                        addLog(LOG_LEVEL_ERROR, F("PZEM004Tv3: Resseting energy failed. Check your command and try again."));
                        break;
                    }
                }
                else
                {
                    addLog(LOG_LEVEL_ERROR, F("PZEM004Tv3: Reset energy. Invalid device address. Must be between 0 and 248 (0xF8)."));
                    break;
                }
            }
            else if(secondVal == "")
            {
                /* No address set, reset all connected devices */
                bool resetOK = false;
                int i = 0;
                for (i = 0; i < (int)P172_PZEM004T_values_vector.size(); i++)
                {
                    if(P172_resetEnergy(P172_PZEM004T_values_vector[i].slave_addr))
                    {
                        String log = F("PZEM004Tv3: Resetting device energy is successful. Used device address is ");
                        log += String(P172_PZEM004T_values_vector[i].slave_addr);
                        log += F(". (");
                        log += String(P172_PZEM004T_values_vector[i].slave_addr, 16);
                        log += F(")");
                        addLog(LOG_LEVEL_INFO, log);
                        resetOK = true;
                    }
                    else
                    {
                        String log = F("PZEM004Tv3: Resetting device energy failed! Used device address is ");
                        log += String(P172_PZEM004T_values_vector[i].slave_addr);
                        log += F(". (");
                        log += String(P172_PZEM004T_values_vector[i].slave_addr, 16);
                        log += F(")");
                        addLog(LOG_LEVEL_ERROR, log);
                    }
                }
                if(!resetOK)
                {
                    addLog(LOG_LEVEL_ERROR, F("PZEM004Tv3: Reset energy. No devices reset!!."));
                    break;
                }
                String log = F("PZEM004Tv3: Resetting devices energy values done. Reset ");
                log += String(i+1);
                log += " device(s).";
                addLog(LOG_LEVEL_INFO, log);
                success = true;
                break;
            }
            addLog(LOG_LEVEL_ERROR, F("PZEM004Tv3: Command failed, unknown error!"));
            break;
        }
        
    }// end pluginwrite

  }// end switch()P172_first_init_done
  return success;
}

#endif //USES_P172