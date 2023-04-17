#include <Arduino.h>
#include <ArduinoBLE.h>
#include <math.h>

#define FS_NANO33BLE_VERSION_MIN_TARGET      "FS_Nano33BLE v1.2.0"
#define FS_NANO33BLE_VERSION_MIN             1002000
#define _FS_LOGLEVEL_               1
// Min NANO33BLE_FS_SIZE_KB must be  64KB. If defined smalller => auto adjust to  64KB
// Max NANO33BLE_FS_SIZE_KB must be 512KB. If defined larger   => auto adjust to 512KB
#define NANO33BLE_FS_SIZE_KB        512
#define FORCE_REFORMAT              false

#include <FS_Nano33BLE.h>

/* BLE stuff */

const char* deviceServiceUuid = "19b10000-e8f2-537e-4f6c-d104768a1214";
const char* coolantTempCharacteristicUuid = "19b10001-e8f2-537e-4f6c-d104768a1214";
const char* AFRCharacteristicUuid = "19b10002-e8f2-537e-4f6c-d104768a1214";

BLEService sensordataService(deviceServiceUuid); 
BLEUnsignedShortCharacteristic coolantTempCharacteristic(coolantTempCharacteristicUuid, BLERead);
BLEUnsignedShortCharacteristic AFRCharacteristic(AFRCharacteristicUuid, BLERead);

/*  Temp sensor stuff */

#define ADC_AVERAGING_LOOPS 3

#define BIAS_RESISTOR_OHMS 2200
#define ADC_REFERENCE_VOLTAGE (3.3f)

// Steinhart-Hart equation coefficients
const static float A = 0.0011234636230968659;                       
const static float B = 0.0003009196475559444;
const static float C = -9.062565347066146e-8;
// Temp sensor characterisation I did with ice and boiling water
// 0c 5574 ohms
// 34c 1337 ohms
// 100c 184 ohms

/* UART Wideband stuff */

#define UART_BUFF_SIZE 8

/* File system stuff */

char dataLog[] = MBED_FS_FILE_PREFIX "/datalog.dat";
char bootCycles[] = MBED_FS_FILE_PREFIX "/bootcycles.txt";

FileSystem_MBED *myFS;

/* Helper functions */

int getTempSensorC(void);
float calculateVoltage(int adc_counts);
float calculateTemperature(float voltage);
bool checkBufIsAFR(uint8_t buf[]);

void setup() {
  Serial.begin(9600);

  analogReadResolution(12);
  delay(500);

  Serial.println("Starting !@!");

  if (!BLE.begin()) {
    Serial.println("- Starting Bluetooth® Low Energy module failed!");
    while (1);
  }
  BLE.setDeviceName("Kfry 944 Sensors");
  BLE.setLocalName("944 Sensor Logger version: 0.1(TySux) (Peripheral)");
  BLE.setAdvertisedService(sensordataService);
  sensordataService.addCharacteristic(coolantTempCharacteristic);
  sensordataService.addCharacteristic(AFRCharacteristic);
  BLE.addService(sensordataService);
  coolantTempCharacteristic.writeValue(0);
  AFRCharacteristic.writeValue(42);
  BLE.advertise();

  Serial.println("Starting 944 sensor logger");

  // myFS = new FileSystem_MBED();

  // if (!myFS->init())
  // {
  //   Serial.println("FS Mount Failed");    
  // }

  // FILE *file = fopen(bootCycles, "+r");
  
  // if (file) 
  // {
  //   Serial.println(" => Open OK");
  // }
  // else
  // {
  //   Serial.println(" => Open Failed");
  //   return;
  // }
 
  // if (fwrite((uint8_t *) message, 1, messageSize, file)) 
  // {
  //   Serial.println("* Writing OK");
  // } 
  // else 
  // {
  //   Serial.println("* Writing failed");
  // }
  
  // fclose(file);

  //Setup serial to read UEGO AFR wideband
  Serial1.begin(9600); // need to invert signal!
}

void loop() {
  int temperatureC = 0;
  uint8_t uartBuf[UART_BUFF_SIZE] = { 0 };
  uint8_t uartBufLen = 0;
  uint8_t incomingByte = 0;
  BLEDevice central = BLE.central();
  uint16_t fixedPointAFR = 0;
  
  while(!central)
  {
    Serial.println("- Discovering central device...");
    delay(500); 
    central = BLE.central();
  }
  Serial.println("- Connected!");
  
  while(central)
  {
    while(Serial1.available() > 0)
    {
      incomingByte = Serial1.read();
      // if(incomingByte == '\r')
      //   Serial.println("got /r");
      // else
      //   Serial.print("(");
      //   Serial.print(uartBufLen);
      //   Serial.print(")");
      //   Serial.print("uint8 print: '");
      //   Serial.print(incomingByte);
      //   Serial.println("'");
      //   Serial.print("char print: '");
      //   Serial.print((char)incomingByte);
      //   Serial.println("'");

      if(incomingByte == '\n') //if complete line recvd
      {        
        if(uartBufLen == 4 && checkBufIsAFR(uartBuf))
        {
          fixedPointAFR = (uartBuf[0]-'0') * 100;
          fixedPointAFR += (uartBuf[1]-'0') * 10;
          fixedPointAFR += (uartBuf[3]-'0');

          AFRCharacteristic.writeValue(fixedPointAFR);
          Serial.println(fixedPointAFR);

        }        
        uartBuf[uartBufLen] = '\0';
        //Serial.println("Complete uart line recvd!!!!: ");
        Serial.println((char *)uartBuf);
        uartBufLen = 0; //reset buffer
      }
      // put byte on buff if theres space, if not drop it 
      else if(uartBufLen < UART_BUFF_SIZE && incomingByte != '\r')
      {
        uartBuf[uartBufLen] = incomingByte;
        uartBufLen++;
      }
    }

    if (central && central.connected()) 
    {
      // Get temperature and update BLE
      temperatureC = getTempSensorC();
      coolantTempCharacteristic.setValue(temperatureC);      
      //TODO set timer to update ble 
      delay(250); 
      
      
    }


  }
  Serial.println("DISconnected!");

}


int getTempSensorC(void)
{
  // read the input on analog pin 0:
  int adcCounts = 0;
  float adcVolts = 0.0;
  float tempertureC = 0.0;
  unsigned long timestamp = 0;

  for(int i=0; i<ADC_AVERAGING_LOOPS; i++)
  {
    adcCounts += analogRead(A1);
  }
  adcCounts = adcCounts / ADC_AVERAGING_LOOPS;

  adcVolts = calculateVoltage(adcCounts);
  tempertureC = calculateTemperature(adcVolts);

  // print out the value you read:
  timestamp = millis();
  Serial.print("Time: ");
  Serial.print(timestamp);
  // Serial.print(" Temperature counts: ");
  // Serial.print(adcCounts);
  // Serial.print(" volts: ");
  // Serial.print(adcVolts);
  Serial.print("   Degrees C: ");
  Serial.print(tempertureC);
  Serial.print("   Degrees C fixed: ");
  Serial.println((int)(tempertureC*10.0));

  return (int)(tempertureC*10.0);
}

float calculateVoltage(int adc_counts)
{
    const float max_adc_counts = 4095.0f;
    const float max_voltage = 3.3f;
    const float voltage_per_count = max_voltage / max_adc_counts;
    float voltage = adc_counts * voltage_per_count;
    return voltage;
}

float calculateTemperature(float voltage)
{
    float resistance; 
    float temperature;
    
    if(ADC_REFERENCE_VOLTAGE - voltage == 0.0)
      resistance = 200000;
    else
      resistance = (voltage * BIAS_RESISTOR_OHMS) / (ADC_REFERENCE_VOLTAGE - voltage);

    temperature = 1 / (A + B * log(resistance) + C * pow(log(resistance), 3));
    temperature -= 273.15; // convert from Kelvin to Celsius
    return temperature;
}

bool checkBufIsAFR(uint8_t buf[])
{
  for(int i=0; i<4; i++)
  {
    if(i == 2)
    {
      if(buf[2] != '.')
        return false;
    }
    else
    {
      if(buf[i] < '0' || buf[i] > '9')
        return false;
    }
  }
  return true;
}
