//#######################################################################################################
//########################### Plugin 118: CCS811 Air Quality TVOC/eCO2 Sensor ###########################
//#######################################################################################################
/*
Plugin written by Alexander Schwantes
Includes sparkfun library https://github.com/sparkfun/SparkFun_CCS811_Arduino_Library

There are various modes for setting up sensor:
 * Interrupt: Requires interrupt pin to signal that a new reading is available. Can read ever 1/10/60 seconds.
 * Wake: Requires a wake pin to wake device for reading when required.
 * Continuous: Takes a reading every 1/10/60 seconds.

This plugin currently implements just the last continuous method as it requires the least number of connected pins.
The library has provisions for the other modes.
*/

#define PLUGIN_118
#define PLUGIN_ID_118         118
#define PLUGIN_NAME_118       "Air Quality - CCS811 TVOC/eCO2 Sensor"
#define PLUGIN_VALUENAME1_118 "TVOC"
#define PLUGIN_VALUENAME2_118 "eCO2"

// int Plugin_118_WAKE_Pin;
// int Plugin_118_INT_Pin;

// #define Plugin_118_nWAKE           2
// #define Plugin_118_nINT            14

#define Plugin_118_D_AWAKE 20  // microseconds to wait before waking waking (deassert) sensor. min 20 microseconds
#define Plugin_118_T_AWAKE 100 // microseconds to wait after waking sensor. min 50 microseconds

/******************************************************************************
CCS811 Arduino library
Marshall Taylor @ SparkFun Electronics
Nathan Seidle @ SparkFun Electronics
April 4, 2017
https://github.com/sparkfun/CCS811_Air_Quality_Breakout
https://github.com/sparkfun/SparkFun_CCS811_Arduino_Library
Resources:
Uses Wire.h for i2c operation
Development environment specifics:
Arduino IDE 1.8.1
This code is released under the [MIT License](http://opensource.org/licenses/MIT).
Please review the LICENSE.md file included with this example. If you have any questions
or concerns with licensing, please contact techsupport@sparkfun.com.
Distributed as-is; no warranty is given.
******************************************************************************/

// **************************************************************************/
// CCS811 Library
// **************************************************************************/
#ifndef __CCS811_H__
# define __CCS811_H__

# include "stdint.h"

// Register addresses
# define CSS811_STATUS          0x00
# define CSS811_MEAS_MODE       0x01
# define CSS811_ALG_RESULT_DATA 0x02
# define CSS811_RAW_DATA        0x03
# define CSS811_ENV_DATA        0x05
# define CSS811_NTC             0x06
# define CSS811_THRESHOLDS      0x10
# define CSS811_BASELINE        0x11
# define CSS811_HW_ID           0x20
# define CSS811_HW_VERSION      0x21
# define CSS811_FW_BOOT_VERSION 0x23
# define CSS811_FW_APP_VERSION  0x24
# define CSS811_ERROR_ID        0xE0
# define CSS811_APP_START       0xF4
# define CSS811_SW_RESET        0xFF

// This is the core operational class of the driver.
//  CCS811Core contains only read and write operations towards the sensor.
//  To use the higher level functions, use the class CCS811 which inherits
//  this class.

class CCS811Core
{
public:
    // Return values
    typedef enum {
        SENSOR_SUCCESS,
        SENSOR_ID_ERROR,
        SENSOR_I2C_ERROR,
        SENSOR_INTERNAL_ERROR,
        SENSOR_GENERIC_ERROR
        // ...
    } status;

    CCS811Core(uint8_t);
    ~CCS811Core() = default;

    status beginCore(void);
    void setAddress(uint8_t);

    // ***Reading functions***//

    // readRegister reads one 8-bit register
    status readRegister(uint8_t offset, uint8_t * outputPointer);
    // multiReadRegister takes a uint8 array address as input and performs
    //  a number of consecutive reads
    status multiReadRegister(uint8_t offset, uint8_t * outputPointer, uint8_t length);

    // ***Writing functions***//

    // Writes an 8-bit byte;
    status writeRegister(uint8_t offset, uint8_t dataToWrite);
    // multiWriteRegister takes a uint8 array address as input and performs
    //  a number of consecutive writes
    status multiWriteRegister(uint8_t offset, uint8_t * inputPointer, uint8_t length);

protected:
    uint8_t I2CAddress;
};

// This is the highest level class of the driver.
//  class CCS811 inherits the CCS811Core and makes use of the beginCore()
// method through its own begin() method.  It also contains user settings/values.

class CCS811 : public CCS811Core
{
public:
    CCS811(uint8_t);

    // Call to check for errors, start app, and set default mode 1
    status begin(void);

    status readAlgorithmResults(void);
    bool checkForStatusError(void);
    bool dataAvailable(void);
    bool appValid(void);
    uint8_t getErrorRegister(void);
    uint16_t getBaseline(void);
    status setBaseline(uint16_t);
    status enableInterrupts(void);
    status disableInterrupts(void);
    status setDriveMode(uint8_t mode);
    status setEnvironmentalData(float relativeHumidity, float temperature);
    void setRefResistance(float);
    status readNTC(void);
    uint16_t getTVOC(void);
    uint16_t getCO2(void);
    float getResistance(void);
    float getTemperature(void);
    String getDriverError(CCS811Core::status);
    String getSensorError(void);

private:
    // These are the air quality values obtained from the sensor
    float refResistance;
    float resistance;
    uint16_t tVOC;
    uint16_t CO2;
    uint16_t vrefCounts = 0;
    uint16_t ntcCounts  = 0;
    float temperature;
};

#endif // End of definition check


CCS811 myCCS811(0x5B); // start with default, but will update later on with user settings

boolean Plugin_118(byte function, struct EventStruct * event, String & string)
{
    boolean success = false;

    switch (function)
    {
        case PLUGIN_DEVICE_ADD:
        {
            Device[++deviceCount].Number           = PLUGIN_ID_118;
            Device[deviceCount].Type               = DEVICE_TYPE_I2C;
            Device[deviceCount].VType              = SENSOR_TYPE_DUAL;
            Device[deviceCount].Ports              = 0;
            Device[deviceCount].PullUpOption       = false;
            Device[deviceCount].InverseLogicOption = false;
            Device[deviceCount].FormulaOption      = true;
            Device[deviceCount].ValueCount         = 2;
            Device[deviceCount].SendDataOption     = true;
            Device[deviceCount].TimerOption        = true;
            break;
        }

        case PLUGIN_GET_DEVICENAME:
        {
            string = F(PLUGIN_NAME_118);
            break;
        }

        case PLUGIN_GET_DEVICEVALUENAMES:
        {
            strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[0], PSTR(PLUGIN_VALUENAME1_118));
            strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[1], PSTR(PLUGIN_VALUENAME2_118));
            break;
        }

        case PLUGIN_WEBFORM_LOAD:
        {
            // I2C address choice
            byte choice = Settings.TaskDevicePluginConfig[event->TaskIndex][0];
            int optionValues[2] = { 0x5A, 0x5B };
            addFormSelectorI2C(string, F("plugin_118_i2c_address"), 2, optionValues, choice);
            addFormNote(string, F("* 0x5A (ADDR pin is LOW), 0x5B (ADDR pin is HIGH)"));

            // read frequency
            int frequencyChoice = (int) Settings.TaskDevicePluginConfigLong[event->TaskIndex][0];
            String frequencyOptions[3] = {F("1 second"), F("10 seconds"), F("60 seconds")};
            int frequencyValues[3] = {1, 2, 3};
            addFormSelector(string, F("Take reading every"), F("plugin_118_read_frequency"), 3,  frequencyOptions, frequencyValues, frequencyChoice);

            addFormSeparator(string);

            // mode
            addFormCheckBox(string, F("Enable temp/humid compensation"), F("plugin_118_enable_compensation"), Settings.TaskDevicePluginConfig[event->TaskIndex][1]);
            addRowLabel(string, F("If this is enabled, the Temperature and Humidity values below need to be configured."));

            // temperature
            addRowLabel(string, F("Temperature:"));
            addTaskSelect(string, F("plugin_118_temperature_task"), Settings.TaskDevicePluginConfig[event->TaskIndex][2]);
            LoadTaskSettings(Settings.TaskDevicePluginConfig[event->TaskIndex][2]); // we need to load the values from another task for selection!
            addRowLabel(string, F("Temperature Value:"));
            addTaskValueSelect(string, F("plugin_118_temperature_value"), Settings.TaskDevicePluginConfig[event->TaskIndex][3], Settings.TaskDevicePluginConfig[event->TaskIndex][2]);

            // temperature scale
            int temperatureScale = Settings.TaskDevicePluginConfig[event->TaskIndex][6];
            String temperatureScaleOptions[2] = {F("&deg;C"), F("&deg;F")};
            int temperatureScaleValues[2] = {0, 1};
            addFormSelector(string, F("Temperature Scale:"), F("plugin_118_temperature_scale"), 2,  temperatureScaleOptions, temperatureScaleValues, temperatureScale);

            // humidity
            addRowLabel(string, F("Humidity:"));
            addTaskSelect(string, F("plugin_118_humidity_task"), Settings.TaskDevicePluginConfig[event->TaskIndex][4]);
            LoadTaskSettings(Settings.TaskDevicePluginConfig[event->TaskIndex][4]); // we need to load the values from another task for selection!
            addRowLabel(string, F("Humidity Value:"));
            addTaskValueSelect(string, F("plugin_118_humidity_value"), Settings.TaskDevicePluginConfig[event->TaskIndex][5], Settings.TaskDevicePluginConfig[event->TaskIndex][4]);

            LoadTaskSettings(event->TaskIndex); // we need to restore our original taskvalues!
            addFormSeparator(string);

            success = true;
            break;
        }

        case PLUGIN_WEBFORM_SAVE:
        {
            Settings.TaskDevicePluginConfig[event->TaskIndex][0] = getFormItemInt(F("plugin_118_i2c_address"));
            Settings.TaskDevicePluginConfig[event->TaskIndex][1] = isFormItemChecked(F("plugin_118_enable_compensation") );
            Settings.TaskDevicePluginConfig[event->TaskIndex][2] = getFormItemInt(F("plugin_118_temperature_task"));
            Settings.TaskDevicePluginConfig[event->TaskIndex][3] = getFormItemInt(F("plugin_118_temperature_value"));
            Settings.TaskDevicePluginConfig[event->TaskIndex][4] = getFormItemInt(F("plugin_118_humidity_task"));
            Settings.TaskDevicePluginConfig[event->TaskIndex][5] = getFormItemInt(F("plugin_118_humidity_value"));
            Settings.TaskDevicePluginConfig[event->TaskIndex][6] = getFormItemInt(F("plugin_118_temperature_scale"));
            Settings.TaskDevicePluginConfigLong[event->TaskIndex][0] = getFormItemInt(F("plugin_118_read_frequency"));

            success = true;
            break;
        }

        case PLUGIN_INIT:
        {
            // Plugin_118_WAKE_Pin = Settings.TaskDevicePin1[event->TaskIndex];
            uint8_t Plugin_118_I2C_ADDR = Settings.TaskDevicePluginConfig[event->TaskIndex][0];
            myCCS811.setAddress(Plugin_118_I2C_ADDR);
            CCS811Core::status returnCode;
            returnCode = myCCS811.begin();
            String log = F("CCS811 : Begin exited with: ");
            log       += myCCS811.getDriverError(returnCode);
            addLog(LOG_LEVEL_DEBUG, log);
            UserVar[event->BaseVarIndex]     = NAN;
            UserVar[event->BaseVarIndex + 1] = NAN;

            // This sets the mode to 1 second reads, and prints returned error status.
            // Mode 0 = Idle (not used)
            // Mode 1 = read every 1s
            // Mode 2 = every 10s
            // Mode 3 = every 60s
            // Mode 4 = RAW mode (not used)
            returnCode = myCCS811.setDriveMode(Settings.TaskDevicePluginConfigLong[event->TaskIndex][0]);
            log        = F("CCS811 : Mode request exited with: ");
            log       += myCCS811.getDriverError(returnCode);
            addLog(LOG_LEVEL_DEBUG, log);

            success = true;
            break;
        }

        case PLUGIN_READ:
        {
            String log;
            // if CCS811 is compensated with temperature and humidity
            if (Settings.TaskDevicePluginConfig[event->TaskIndex][1])
            {
                // we're checking a var from another task, so calculate that basevar
                byte TaskIndex    = Settings.TaskDevicePluginConfig[event->TaskIndex][2];
                byte BaseVarIndex = TaskIndex * VARS_PER_TASK + Settings.TaskDevicePluginConfig[event->TaskIndex][3];
                float temperature = UserVar[BaseVarIndex]; // in degrees C
                // convert to celsius if required
                int temperature_in_fahrenheit = Settings.TaskDevicePluginConfig[event->TaskIndex][6];
                String temp = F("°C");
                if (temperature_in_fahrenheit)
                {
                    temperature = (temperature - 32) * 5 / 9;
                    temp =  F("°F");
                }

                byte TaskIndex2    = Settings.TaskDevicePluginConfig[event->TaskIndex][4];
                byte BaseVarIndex2 = TaskIndex2 * VARS_PER_TASK + Settings.TaskDevicePluginConfig[event->TaskIndex][5];
                float humidity     = UserVar[BaseVarIndex2]; // in % relative

                log = F("CCS811 : Compensating for Temperature: ");
                log += String(temperature) + temp + F(" & Humidity: ") + String(humidity) + F("%");
                addLog(LOG_LEVEL_DEBUG, log);

                myCCS811.setEnvironmentalData(humidity, temperature);

                //  myCCS811.readAlgorithmResults(); //Dump a reading and wait
                delay(100);
            }

            if (myCCS811.dataAvailable() )
            {
                // Calling readAlgorithmResults() function updates the global tVOC and CO2 variables
                CCS811Core::status readstatus = myCCS811.readAlgorithmResults();
                if (readstatus == 0)
                {
                    UserVar[event->BaseVarIndex]     = myCCS811.getTVOC();
                    UserVar[event->BaseVarIndex + 1] = myCCS811.getCO2();
                    success = true;

                    log  = F("CCS811 : tVOC: ");
                    log += myCCS811.getTVOC();
                    log += F(", eCO2: ");
                    log += myCCS811.getCO2();
                    addLog(LOG_LEVEL_INFO, log);
                }
                else
                {
                    log  = F("CCS811 : Error reading values : ");
                    log += readstatus;
                    addLog(LOG_LEVEL_ERROR, log);
                }
            }
            else if (myCCS811.checkForStatusError() )
            {
                // If the CCS811 found an internal error, print it.
                log  = F("CCS811 : Error: ");
                log += myCCS811.getSensorError();
                addLog(LOG_LEVEL_ERROR, log);
            }
            else
            {
                log = F("CCS811 : No values found.");
                addLog(LOG_LEVEL_ERROR, log);
            }

            if(!success)
            {
                UserVar[event->BaseVarIndex]     = NAN;
                UserVar[event->BaseVarIndex + 1] = NAN;
            }

            break;
        }
    } // switch

    return success;
} // Plugin_118



// ****************************************************************************//
//
//  LIS3DHCore functions
//
//  For I2C, construct LIS3DHCore myIMU(<address>);
//
//  Default <address> is 0x5B.
//
// ****************************************************************************//
CCS811Core::CCS811Core(uint8_t inputArg) : I2CAddress(inputArg)
{
}

void CCS811Core::setAddress(uint8_t address)
{
    I2CAddress = address;
}

CCS811Core::status CCS811Core::beginCore(void)
{
    CCS811Core::status returnError = SENSOR_SUCCESS;

    // Wire.begin(); // not necessary

    #ifdef __AVR__
    #else
    #endif

    #ifdef __MK20DX256__
    #else
    #endif

    #ifdef ARDUINO_ARCH_ESP8266
    #else
    #endif

    // Spin for a few ms
    volatile uint8_t temp = 0;
    for (uint16_t i = 0; i < 10000; i++)
    {
        temp++;
    }

    while (Wire.available() ) // Clear wire as a precaution
    {
        Wire.read();
    }

    // Check the ID register to determine if the operation was a success.
    uint8_t readCheck;
    readCheck   = 0;
    returnError = readRegister(CSS811_HW_ID, &readCheck);

    if (returnError != SENSOR_SUCCESS)
    {
        return returnError;
    }

    if (readCheck != 0x81)
    {
        returnError = SENSOR_ID_ERROR;
    }

    return returnError;
} // CCS811Core::beginCore

// ****************************************************************************//
//
//  ReadRegister
//
//  Parameters:
//    offset -- register to read
//    *outputPointer -- Pass &variable (address of) to save read data to
//
// ****************************************************************************//
CCS811Core::status CCS811Core::readRegister(uint8_t offset, uint8_t * outputPointer)
{
    // Return value
    uint8_t result = 0;
    uint8_t numBytes = 1;

    CCS811Core::status returnError = SENSOR_SUCCESS;
    Wire.beginTransmission(I2CAddress);
    Wire.write(offset);
    if (Wire.endTransmission() != 0)
    {
        returnError = SENSOR_I2C_ERROR;
    }

    Wire.requestFrom(I2CAddress, numBytes);
    while (Wire.available() ) // slave may send less than requested
    {
        result = Wire.read(); // receive a byte as a proper uint8_t
    }

    *outputPointer = result;

    return returnError;
}

// ****************************************************************************//
//
//  multiReadRegister
//
//  Parameters:
//    offset -- register to read
//    *outputPointer -- Pass &variable (base address of) to save read data to
//    length -- number of bytes to read
//
//  Note:  Does not know if the target memory space is an array or not, or
//    if there is the array is big enough.  if the variable passed is only
//    two bytes long and 3 bytes are requested, this will over-write some
//    other memory!
//
// ****************************************************************************//
CCS811Core::status CCS811Core::multiReadRegister(uint8_t offset, uint8_t * outputPointer, uint8_t length)
{
    CCS811Core::status returnError = SENSOR_SUCCESS;

    // define pointer that will point to the external space
    uint8_t i = 0;
    uint8_t c = 0;
    // Set the address
    Wire.beginTransmission(I2CAddress);
    Wire.write(offset);
    if (Wire.endTransmission() != 0)
    {
        returnError = SENSOR_I2C_ERROR;
    }
    else // OK, all worked, keep going
    {    // request 6 bytes from slave device
        Wire.requestFrom(I2CAddress, length);
        while ( ( Wire.available() ) && (i < length) ) // slave may send less than requested
        {
            c = Wire.read(); // receive a byte as character
            *outputPointer = c;
            outputPointer++;
            i++;
        }

        // dump extra
        while (Wire.available() )
        {
            Wire.read();
        }
    }

    return returnError;
} // CCS811Core::multiReadRegister

// ****************************************************************************//
//
//  writeRegister
//
//  Parameters:
//    offset -- register to write
//    dataToWrite -- 8 bit data to write to register
//
// ****************************************************************************//
CCS811Core::status CCS811Core::writeRegister(uint8_t offset, uint8_t dataToWrite)
{
    CCS811Core::status returnError = SENSOR_SUCCESS;

    Wire.beginTransmission(I2CAddress);
    Wire.write(offset);
    Wire.write(dataToWrite);
    if (Wire.endTransmission() != 0)
    {
        returnError = SENSOR_I2C_ERROR;
    }

    return returnError;
}

// ****************************************************************************//
//
//  multiReadRegister
//
//  Parameters:
//    offset -- register to read
//    *inputPointer -- Pass &variable (base address of) to save read data to
//    length -- number of bytes to read
//
//  Note:  Does not know if the target memory space is an array or not, or
//    if there is the array is big enough.  if the variable passed is only
//    two bytes long and 3 bytes are requested, this will over-write some
//    other memory!
//
// ****************************************************************************//
CCS811Core::status CCS811Core::multiWriteRegister(uint8_t offset, uint8_t * inputPointer, uint8_t length)
{
    CCS811Core::status returnError = SENSOR_SUCCESS;
    // define pointer that will point to the external space
    uint8_t i = 0;
    // Set the address
    Wire.beginTransmission(I2CAddress);
    Wire.write(offset);
    while (i < length) // send data bytes
    {
        Wire.write(*inputPointer); // receive a byte as character
        inputPointer++;
        i++;
    }

    if (Wire.endTransmission() != 0)
    {
        returnError = SENSOR_I2C_ERROR;
    }

    return returnError;
}

// ****************************************************************************//
//
//  Main user class -- wrapper for the core class + maths
//
//  Construct with same rules as the core ( uint8_t busType, uint8_t inputArg )
//
// ****************************************************************************//
CCS811::CCS811(uint8_t inputArg) : CCS811Core(inputArg)
{
    refResistance = 10000;
    resistance    = 0;
    temperature   = 0;
    tVOC = 0;
    CO2  = 0;
}

// ****************************************************************************//
//
//  Begin
//
//  This starts the lower level begin, then applies settings
//
// ****************************************************************************//
CCS811Core::status CCS811::begin(void)
{
    uint8_t data[4] = { 0x11, 0xE5, 0x72, 0x8A }; // Reset key

    CCS811Core::status returnError = SENSOR_SUCCESS; // Default error state

    // restart the core
    returnError = beginCore();
    if (returnError != SENSOR_SUCCESS)
    {
        return returnError;
    }

    // Reset the device
    multiWriteRegister(CSS811_SW_RESET, data, 4);
    // Tclk = 1/16MHz = 0x0000000625
    // 0.001 s / tclk = 16000 counts
    volatile uint8_t temp = 0;
    #ifdef ARDUINO_ARCH_ESP8266
    for (uint32_t i = 0; i < 80000; i++) // This waits > 1ms @ 80MHz clock
    {
        temp++;
    }
    #elif __AVR__
    for (uint16_t i = 0; i < 16000; i++) // This waits > 1ms @ 16MHz clock
    {
        temp++;
    }
    #else // ifdef ARDUINO_ARCH_ESP8266
    for (uint32_t i = 0; i < 200000; i++)     // Spin for a good while
    {
        temp++;
    }
    #endif // ifdef ARDUINO_ARCH_ESP8266
    if (checkForStatusError() == true)
    {
        return SENSOR_INTERNAL_ERROR;
    }

    if (appValid() == false)
    {
        return SENSOR_INTERNAL_ERROR;
    }

    // Write 0 bytes to this register to start app
    Wire.beginTransmission(I2CAddress);
    Wire.write(CSS811_APP_START);
    if (Wire.endTransmission() != 0)
    {
        return SENSOR_I2C_ERROR;
    }

    delay(200);

    // returnError = setDriveMode(1); //Read every second
    Serial.println();

    return returnError;
} // CCS811::begin

// ****************************************************************************//
//
//  Sensor functions
//
// ****************************************************************************//
// Updates the total voltatile organic compounds (TVOC) in parts per billion (PPB)
// and the CO2 value
// Returns nothing
CCS811Core::status CCS811::readAlgorithmResults(void)
{
    uint8_t data[4];

    CCS811Core::status returnError = multiReadRegister(CSS811_ALG_RESULT_DATA, data, 4);
    if (returnError != SENSOR_SUCCESS)
    {
        return returnError;
    }

    // Data ordered:
    // co2MSB, co2LSB, tvocMSB, tvocLSB

    CO2  = ( (uint16_t) data[0] << 8 ) | data[1];
    tVOC = ( (uint16_t) data[2] << 8 ) | data[3];
    return SENSOR_SUCCESS;
}

// Checks to see if error bit is set
bool CCS811::checkForStatusError(void)
{
    uint8_t value;

    // return the status bit
    readRegister(CSS811_STATUS, &value);
    return (value & 1 << 0);
}

// Checks to see if DATA_READ flag is set in the status register
bool CCS811::dataAvailable(void)
{
    uint8_t value;

    CCS811Core::status returnError = readRegister(CSS811_STATUS, &value);
    if (returnError != SENSOR_SUCCESS)
    {
        return 0;
    }
    else
    {
        return (value & 1 << 3);
    }
}

// Checks to see if APP_VALID flag is set in the status register
bool CCS811::appValid(void)
{
    uint8_t value;

    CCS811Core::status returnError = readRegister(CSS811_STATUS, &value);
    if (returnError != SENSOR_SUCCESS)
    {
        return 0;
    }
    else
    {
        return (value & 1 << 4);
    }
}

uint8_t CCS811::getErrorRegister(void)
{
    uint8_t value;

    CCS811Core::status returnError = readRegister(CSS811_ERROR_ID, &value);
    if (returnError != SENSOR_SUCCESS)
    {
        return 0xFF;
    }
    else
    {
        return value; // Send all errors in the event of communication error
    }
}

// Returns the baseline value
// Used for telling sensor what 'clean' air is
// You must put the sensor in clean air and record this value
uint16_t CCS811::getBaseline(void)
{
    uint8_t data[2];

    CCS811Core::status returnError = multiReadRegister(CSS811_BASELINE, data, 2);

    unsigned int baseline = ( (uint16_t) data[0] << 8 ) | data[1];
    if (returnError != SENSOR_SUCCESS)
    {
        return 0;
    }
    else
    {
        return (baseline);
    }
}

CCS811Core::status CCS811::setBaseline(uint16_t input)
{
    uint8_t data[2];

    data[0] = (input >> 8) & 0x00FF;
    data[1] = input & 0x00FF;

    CCS811Core::status returnError = multiWriteRegister(CSS811_BASELINE, data, 2);

    return returnError;
}

// Enable the nINT signal
CCS811Core::status CCS811::enableInterrupts(void)
{
    uint8_t value;

    CCS811Core::status returnError = readRegister(CSS811_MEAS_MODE, &value); // Read what's currently there
    if (returnError != SENSOR_SUCCESS)
    {
        return returnError;
    }

    Serial.println(value, HEX);
    value |= (1 << 3); // Set INTERRUPT bit
    writeRegister(CSS811_MEAS_MODE, value);
    Serial.println(value, HEX);
    return returnError;
}

// Disable the nINT signal
CCS811Core::status CCS811::disableInterrupts(void)
{
    uint8_t value;

    CCS811Core::status returnError = readRegister(CSS811_MEAS_MODE, &value); // Read what's currently there
    if (returnError != SENSOR_SUCCESS)
    {
        return returnError;
    }

    value      &= ~(1 << 3); // Clear INTERRUPT bit
    returnError = writeRegister(CSS811_MEAS_MODE, value);
    return returnError;
}

// Mode 0 = Idle
// Mode 1 = read every 1s
// Mode 2 = every 10s
// Mode 3 = every 60s
// Mode 4 = RAW mode
CCS811Core::status CCS811::setDriveMode(uint8_t mode)
{
    if (mode > 4)
    {
        mode = 4; // sanitize input
    }

    uint8_t value;
    CCS811Core::status returnError = readRegister(CSS811_MEAS_MODE, &value); // Read what's currently there
    if (returnError != SENSOR_SUCCESS)
    {
        return returnError;
    }

    value      &= ~(0b00000111 << 4); // Clear DRIVE_MODE bits
    value      |= (mode << 4);        // Mask in mode
    returnError = writeRegister(CSS811_MEAS_MODE, value);
    return returnError;
}

// Given a temp and humidity, write this data to the CSS811 for better compensation
// This function expects the humidity and temp to come in as floats
CCS811Core::status CCS811::setEnvironmentalData(float relativeHumidity, float temperature)
{
    // Check for invalid temperatures
    if ( (temperature < -25) || (temperature > 50) )
    {
        return SENSOR_GENERIC_ERROR;
    }

    // Check for invalid humidity
    if ( (relativeHumidity < 0) || (relativeHumidity > 100) )
    {
        return SENSOR_GENERIC_ERROR;
    }

    uint32_t rH   = relativeHumidity * 1000; // 42.348 becomes 42348
    uint32_t temp = temperature * 1000;      // 23.2 becomes 23200

    byte envData[4];

    // Split value into 7-bit integer and 9-bit fractional
    envData[0] = ( (rH % 1000) / 100 ) > 7 ? (rH / 1000 + 1) << 1 : (rH / 1000) << 1;
    envData[1] = 0; // CCS811 only supports increments of 0.5 so bits 7-0 will always be zero
    if ( ( (rH % 1000) / 100 ) > 2 && ( ( (rH % 1000) / 100 ) < 8 ) )
    {
        envData[0] |= 1; // Set 9th bit of fractional to indicate 0.5%
    }

    temp += 25000; // Add the 25C offset
    // Split value into 7-bit integer and 9-bit fractional
    envData[2] = ( (temp % 1000) / 100 ) > 7 ? (temp / 1000 + 1) << 1 : (temp / 1000) << 1;
    envData[3] = 0;
    if ( ( (temp % 1000) / 100 ) > 2 && ( ( (temp % 1000) / 100 ) < 8 ) )
    {
        envData[2] |= 1; // Set 9th bit of fractional to indicate 0.5C
    }

    CCS811Core::status returnError = multiWriteRegister(CSS811_ENV_DATA, envData, 4);
    return returnError;
} // CCS811::setEnvironmentalData

void CCS811::setRefResistance(float input)
{
    refResistance = input;
}

CCS811Core::status CCS811::readNTC(void)
{
    uint8_t data[4];

    CCS811Core::status returnError = multiReadRegister(CSS811_NTC, data, 4);

    vrefCounts = ( (uint16_t) data[0] << 8 ) | data[1];
    // Serial.print("vrefCounts: ");
    // Serial.println(vrefCounts);
    ntcCounts = ( (uint16_t) data[2] << 8 ) | data[3];
    // Serial.print("ntcCounts: ");
    // Serial.println(ntcCounts);
    // Serial.print("sum: ");
    // Serial.println(ntcCounts + vrefCounts);
    resistance = ( (float) ntcCounts * refResistance / (float) vrefCounts );

    // Code from Milan Malesevic and Zoran Stupic, 2011,
    // Modified by Max Mayfield,
    temperature = log( (long) resistance);
    temperature = 1  / ( 0.001129148 + (0.000234125 * temperature) + (0.0000000876741 * temperature * temperature * temperature) );
    temperature = temperature - 273.15; // Convert Kelvin to Celsius

    return returnError;
}

uint16_t CCS811::getTVOC(void)
{
    return tVOC;
}

uint16_t CCS811::getCO2(void)
{
    return CO2;
}

float CCS811::getResistance(void)
{
    return resistance;
}

float CCS811::getTemperature(void)
{
    return temperature;
}

// getDriverError decodes the CCS811Core::status type and prints the
// type of error to the serial terminal.
//
// Save the return value of any function of type CCS811Core::status, then pass
// to this function to see what the output was.
String CCS811::getDriverError(CCS811Core::status errorCode)
{
    switch (errorCode)
    {
        case CCS811Core::SENSOR_SUCCESS:
            return "SUCCESS";

        case CCS811Core::SENSOR_ID_ERROR:
            return "ID_ERROR";

        case CCS811Core::SENSOR_I2C_ERROR:
            return "I2C_ERROR";

        case CCS811Core::SENSOR_INTERNAL_ERROR:
            return "INTERNAL_ERROR";

        case CCS811Core::SENSOR_GENERIC_ERROR:
            return "GENERIC_ERROR";

        default:
            return "Unspecified error.";
    }
}

// getSensorError gets, clears, then prints the errors
// saved within the error register.
String CCS811::getSensorError()
{
    uint8_t error = getErrorRegister();

    if (error == 0xFF)
    {
        return "Failed to get ERROR_ID register.";
    }
    else
    {
        if (error & 1 << 5)
        {
            return "HeaterSupply";
        }

        if (error & 1 << 4)
        {
            return "HeaterFault";
        }

        if (error & 1 << 3)
        {
            return "MaxResistance";
        }

        if (error & 1 << 2)
        {
            return "MeasModeInvalid";
        }

        if (error & 1 << 1)
        {
            return "ReadRegInvalid";
        }

        if (error & 1 << 0)
        {
            return "MsgInvalid";
        }
        return "Unkown Error";
    }
}
