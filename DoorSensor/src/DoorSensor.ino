#include "MPU9150.h"
#include "Si1132.h"
#include "Si70xx.h"
#include "math.h"


//// ***************************************************************************
//// ***************************************************************************

//// Initialize application variables
#define RAD_TO_DEGREES 57.2957795131
#define DEG_TO_RADIANS 0.0174533
#define PI 3.1415926535
#define ACCEL_SCALE 2 // +/- 2g
#define COMP_I2C_ADDRESS  0x0C;      //change Adress to Compass  // R/W

//MPU9150 Compass
#define MPU9150_CMPS_XOUT_L        0x4A   // R
#define MPU9150_CMPS_XOUT_H        0x4B   // R
#define MPU9150_CMPS_YOUT_L        0x4C   // R
#define MPU9150_CMPS_YOUT_H        0x4D   // R
#define MPU9150_CMPS_ZOUT_L        0x4E   // R
#define MPU9150_CMPS_ZOUT_H        0x4F   // R

int SENSORDELAY = 500;  //// 500; //3000; // milliseconds (runs x1)
int EVENTSDELAY = 1000; //// milliseconds (runs x10)
int OTAUPDDELAY = 7000; //// milliseconds (runs x1)
int SLEEP_DELAY = 0;    //// 40 seconds (runs x1) - should get about 24 hours on 2100mAH, 0 to disable and use RELAX_DELAY instead
String SLEEP_DELAY_MIN = "15"; // seconds - easier to store as string then convert to int
String SLEEP_DELAY_STATUS = "OK"; // always OK to start with
int RELAX_DELAY = 5; // seconds (runs x1) - no power impact, just idle/relaxing

// Variables for the I2C scan
byte I2CERR, I2CADR;

//// ***************************************************************************
//// ***************************************************************************

int I2CEN = D2;
int ALGEN = D3;
int LED = D7;

int SOUND = A0;
double SOUNDV = 0; //// Volts Peak-to-Peak Level/Amplitude

int POWR1 = A1;
int POWR2 = A2;
int POWR3 = A3;
double POWR1V = 0; //Watts
double POWR2V = 0; //Watts
double POWR3V = 0; //Watts

int SOILT = A4;
double SOILTV = 0; //// Celsius: temperature (C) = Vout*41.67-40 :: Temperature (F) = Vout*75.006-40

int SOILH = A5;
double SOILHV = 0; //// Volumetric Water Content (VWC): http://www.vegetronix.com/TechInfo/How-To-Measure-VWC.phtml

bool BMP180OK = false;
double BMP180Pressure = 0;    //// hPa
double BMP180Temperature = 0; //// Celsius
double BMP180Altitude = 0;    //// Meters

bool Si7020OK = false;
double Si7020Temperature = 0; //// Celsius
double Si7020Humidity = 0;    //// %Relative Humidity

bool Si1132OK = false;
double Si1132UVIndex = 0; //// UV Index scoring is as follows: 1-2  -> Low, 3-5  -> Moderate, 6-7  -> High, 8-10 -> Very High, 11+  -> Extreme
double Si1132Visible = 0; //// Lux
double Si1132InfraRd = 0; //// Lux

MPU9150 mpu9150;
bool ACCELOK = false;
int cx, cy, cz, ax, ay, az, gx, gy, gz;
double tm; //// Celsius

float pitch = 0;
float roll = 0;
float yaw = 0;

float compassX = 0;
float compassY = 0;
float compassZ = 0;

float originalX = 0;
float originalY = 0;
float originalZ = 0;

String DOORINFTIME = "";
String DOORCOMTIME = "";
bool lock = false;

int mag[3]; //*******

Si1132 si1132 = Si1132();

// I2C address 0x69 could be 0x68 depends on your wiring.
int MPU9150_I2C_ADDRESS = 0x68;

//// ***************************************************************************
//// ***************************************************************************


///SYSTEM_MODE(SEMI_AUTOMATIC);

float mapFloat(float x, float in_min, float in_max, float out_min, float out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void setPinsMode()
{
    pinMode(I2CEN, OUTPUT);
    pinMode(ALGEN, OUTPUT);
    pinMode(LED, OUTPUT);

    pinMode(SOUND, INPUT);

    pinMode(POWR1, INPUT);
    pinMode(POWR2, INPUT);
    pinMode(POWR3, INPUT);

    pinMode(SOILT, INPUT);
    pinMode(SOILH, INPUT);
}

void setup()
{
    // opens serial over USB
    Serial.begin(9600);

    // Set I2C speed
    // 400Khz seems to work best with the Photon with the packaged I2C sensors
    Wire.setSpeed(CLOCK_SPEED_400KHZ);

    Wire.begin();  // Start up I2C, required for LSM303 communication

    // diables interrupts
    noInterrupts();

    // initialises the IO pins
    setPinsMode();

    // initialises MPU9150 inertial measure unit
    MPU9150_I2C_ADDRESS = 0x68;
    MPU9150_writeSensor(0x6B,0x01);            // PLL with X axis gyroscope reference and disable sleep mode
    MPU9150_writeSensor(MPU9150_PWR_MGMT_1, 0); // Clear the 'sleep' bit to start the sensor.

    initialiseMPU9150();
    readMPU9150();          // reads compass, accelerometer and gyroscope

    originalX = getCompassX(cx);
    originalY = getCompassY(cy);
    originalZ = getCompassZ(cz);

    Particle.subscribe("DOORINF", DOORINF);
}

void initialiseMPU9150()
{
  ACCELOK = mpu9150.begin(mpu9150._addr_motion); // Initialize MPU9150
  // Clear the 'sleep' bit to start the sensor.
  mpu9150.writeSensor(mpu9150._addr_motion, 0x6B, 0x01);
  mpu9150.writeSensor(mpu9150._addr_motion, MPU9150_PWR_MGMT_1, 0);


  if (ACCELOK)
  {
      //mpu9150.writeSensor(mpu9150._addr_motion, MPU9150_PWR_MGMT_1, 0);

      //set compass
      mpu9150.writeSensor(mpu9150._addr_compass, 0x0A, 0x00); //Powerdown mode
      mpu9150.writeSensor(mpu9150._addr_compass, 0x0A, 0x0F); //Self test
      mpu9150.writeSensor(mpu9150._addr_compass, 0x0A, 0x00); //Powerdown mode

      mpu9150.writeSensor(mpu9150._addr_motion, 0x24, 0x40); //Wait for Data at Slave0
      mpu9150.writeSensor(mpu9150._addr_motion, 0x25, 0x8C); //Set i2c address at slave0 at 0x0C
      mpu9150.writeSensor(mpu9150._addr_motion, 0x26, 0x02); //Set where reading at slave 0 starts
      mpu9150.writeSensor(mpu9150._addr_motion, 0x27, 0x88); //set offset at start reading and enable
      mpu9150.writeSensor(mpu9150._addr_motion, 0x28, 0x0C); //set i2c address at slv1 at 0x0C
      mpu9150.writeSensor(mpu9150._addr_motion, 0x29, 0x0A); //Set where reading at slave 1 starts
      mpu9150.writeSensor(mpu9150._addr_motion, 0x2A, 0x81); //Enable at set length to 1
      mpu9150.writeSensor(mpu9150._addr_motion, 0x64, 0x01); //overvride register
      mpu9150.writeSensor(mpu9150._addr_motion, 0x67, 0x03); //set delay rate
      mpu9150.writeSensor(mpu9150._addr_motion, 0x01, 0x80);

      mpu9150.writeSensor(mpu9150._addr_motion, 0x34, 0x04); //set i2c slv4 delay
      mpu9150.writeSensor(mpu9150._addr_motion, 0x64, 0x00); //override register
      mpu9150.writeSensor(mpu9150._addr_motion, 0x6A, 0x00); //clear usr setting
      mpu9150.writeSensor(mpu9150._addr_motion, 0x64, 0x01); //override register
      mpu9150.writeSensor(mpu9150._addr_motion, 0x6A, 0x20); //enable master i2c mode
      mpu9150.writeSensor(mpu9150._addr_motion, 0x34, 0x13); //disable slv4

      delay(100);
    }
    else
    {
      Serial.println("Unable to start MPU5150");
    }

}

void loop(void)
{
    //// prints device version and address
    /*
    Serial.print("Device version: "); Serial.println(System.version());
    Serial.print("Device ID: "); Serial.println(System.deviceID());
    Serial.print("WiFi IP: "); Serial.println(WiFi.localIP());
    */

    //// powers up sensors
    digitalWrite(I2CEN, HIGH);
    digitalWrite(ALGEN, HIGH);

    //// allows sensors time to warm up
    delay(SENSORDELAY);

    readMPU9150();          // reads compass, accelerometer and gyroscope

    compassX = getCompassX(cx);
    compassY = getCompassY(cy);
    compassZ = getCompassZ(cz);

    String compassString = "";
    compassString = compassString+"X: "+compassX+" Y: "+compassY+" Z: "+compassZ;

    String originalCompassString = "";
    originalCompassString = originalCompassString+"X: "+originalX+" Y: "+originalY+" Z: "+originalZ;

    //determines if the door has been moved
    if(compassX > originalX + 1.5 || compassY > originalY + 1.5 || compassZ > originalZ + 2) {
      //WiFi.on();
      //while(WiFi.connecting()){} //delays until WiFi is connected before progressing further down the if loop
      //delay(1000);
      digitalWrite(LED, HIGH);
      Particle.publish("Compass", compassString, PRIVATE);
      Particle.publish("OriginalCompass", originalCompassString, PRIVATE);
      String DOORCOMTIME = String(Time.now());
      DOORCOM(DOORCOMTIME);
      delay(1000);
    }
    digitalWrite(LED, LOW);
    //delay(1000);
    //WiFi.off();

    /*
    if(compassX > originalX + 1.25 || compassY > originalY + 1.25 || compassZ > originalZ + 1.25){
      WiFi.connect();
      while(WiFi.connecting()){ //delays until WiFi is connected before progressing further down the if loop
        delay(1000);
      }
      delay(1000);
      Particle.publish("Compass", compassString, PRIVATE);
      delay(1000);
    }
    WiFi.disconnect();
    */

    /*Publish to Thingspeak and populate fields 1,2,3 accordingly
    we specify event name thingSpeakWrite_All that is recognised by the webhook we integrated with our project
    on the particle dashboard.*/

    //delay(SLEEP_DELAY); //Stay awake for a while

    // Power Down Sensors
    //digitalWrite(I2CEN, LOW);
    //digitalWrite(ALGEN, LOW);
}

void DOORINF(const char *event, const char *data)
{
  DOORINFTIME = data;
  Particle.publish("Infrared moved: ", DOORINFTIME, PRIVATE);
  delay(5000);
  myHandler(DOORINFTIME, DOORCOMTIME);
}

void DOORCOM(String DOORCOMTIME)
{
  Particle.publish("Compass moved: ", DOORCOMTIME, PRIVATE);
  delay(5000);
  myHandler(DOORINFTIME, DOORCOMTIME);
}

void myHandler(String DOORINFTIME, String DOORCOMTIME)
{
  /* Particle.subscribe handlers are void functions, which means they don't return anything.
  They take two variables-- the name of your event, and any data that goes along with your event.
  */
  if (strcmp(DOORINFTIME,DOORCOMTIME) < 0 && strcmp(DOORINFTIME,DOORCOMTIME) > 10) {
    Particle.publish("LEAVING", strcmp(DOORINFTIME,DOORCOMTIME));
    Serial.println("LEAVING!");
    Particle.publish("Leaving detected", DOORCOMTIME, PRIVATE);
  }
  else if (strcmp(DOORINFTIME,DOORCOMTIME) > 0 && strcmp(DOORINFTIME,DOORCOMTIME) < -10) {
    Particle.publish("ENTERING", strcmp(DOORINFTIME,DOORCOMTIME));
    Serial.println("ENTERING!");
    Particle.publish("Entering detected", DOORINFTIME, PRIVATE);
  }
  else {
    Serial.println("One sensor detected motion, but the other didn't. Avoiding anomaly.");
  }
  Particle.publish("--------------------");
}

void readMPU9150()
{
    //// reads the MPU9150 sensor values. Values are read in order of temperature,
    //// compass, Gyro, Accelerometer

    tm = ( (double) mpu9150.readSensor(mpu9150._addr_motion, MPU9150_TEMP_OUT_L, MPU9150_TEMP_OUT_H) + 12412.0 ) / 340.0;

    //cx = mpu9150.readSensor(mpu9150._addr_motion, MPU9150_CMPS_XOUT_L,MPU9150_CMPS_XOUT_H);
    //cy = mpu9150.readSensor(mpu9150._addr_motion, MPU9150_CMPS_YOUT_L,MPU9150_CMPS_YOUT_H);
    //cz = mpu9150.readSensor(mpu9150._addr_motion, MPU9150_CMPS_ZOUT_L,MPU9150_CMPS_ZOUT_H);

    cx = mpu9150.readSensor(mpu9150._addr_motion, MPU9150_CMPS_XOUT_L, MPU9150_CMPS_XOUT_H);
    cy = mpu9150.readSensor(mpu9150._addr_motion, MPU9150_CMPS_YOUT_L, MPU9150_CMPS_YOUT_H);
    cz = mpu9150.readSensor(mpu9150._addr_motion, MPU9150_CMPS_ZOUT_L, MPU9150_CMPS_ZOUT_H);

    ax = mpu9150.readSensor(mpu9150._addr_motion, MPU9150_ACCEL_XOUT_L, MPU9150_ACCEL_XOUT_H);
    ay = mpu9150.readSensor(mpu9150._addr_motion, MPU9150_ACCEL_YOUT_L, MPU9150_ACCEL_YOUT_H);
    az = mpu9150.readSensor(mpu9150._addr_motion, MPU9150_ACCEL_ZOUT_L, MPU9150_ACCEL_ZOUT_H);

    gx = mpu9150.readSensor(mpu9150._addr_motion, MPU9150_GYRO_XOUT_L, MPU9150_GYRO_XOUT_H);
    gy = mpu9150.readSensor(mpu9150._addr_motion, MPU9150_GYRO_YOUT_L, MPU9150_GYRO_YOUT_H);
    gz = mpu9150.readSensor(mpu9150._addr_motion, MPU9150_GYRO_ZOUT_L, MPU9150_GYRO_ZOUT_H);
}

int readWeatherSi7020()
{
    Si70xx si7020;
    Si7020OK = si7020.begin(); //// initialises Si7020

    if (Si7020OK)
    {
        Si7020Temperature = si7020.readTemperature();
        Si7020Humidity = si7020.readHumidity();
    }

    return Si7020OK ? 2 : 0;
}


//// returns accelaration along x-axis, should be 0-1g
float getAccelX(float x)
{
  return x/pow(2,15)*ACCEL_SCALE;
}

//// returns accelaration along z-axis, should be 0-1g
float getAccelY(float y)
{
  return y/pow(2,15)*ACCEL_SCALE;
}

//// returns accelaration along z-axis should be 0-1g
float getAccelZ(float z)
{
  return z/pow(2,15)*ACCEL_SCALE;
}

//// returns the vector sum of the acceleration along x, y and z axes
//// in g units
float getAccelXYZ(float x, float y, float z)
{
  x = getAccelX(x);
  y = getAccelY(y);
  z = getAccelZ(z);

  return sqrt(x*x+y*y+z*z);
}

float getRoll(float accelX, float accelY, float accelZ)
{
  return atan2(accelY , sqrt(accelX*accelX+accelZ*accelZ))*RAD_TO_DEGREES;
}


float getPitch(float accelX, float accelY, float accelZ)
{
  return atan2(accelX , sqrt(accelY*accelY+accelZ*accelZ))*RAD_TO_DEGREES;
}


//// returns tilt along x axis in radians - uses accelerometer
float getXTilt(float accelX, float accelZ)
{


   float tilt = atan2(accelX,accelZ)*RAD_TO_DEGREES; //*RAD_TO_DEGREES;
   if(tilt < 0)
   {
      tilt = tilt+360.0;
   }
   return tilt*DEG_TO_RADIANS;
  //return tilt;
}

//// returns tilt along y-axis in radians
float getYTilt(float accelY, float accelZ)
{
   float tilt = atan2(accelY,accelZ)*RAD_TO_DEGREES;
   if(tilt < 0)
   {
     tilt = tilt+360.0;
   }
   return tilt*DEG_TO_RADIANS;
   //return tilt;
}

float getCompassX(float x)
{
  return x *0.3;
}

float getCompassY(float y)
{
  return y*0.3;
}

float getCompassZ(float z)
{
  return z*0.4;
}

float readSoundLevel()
{
    unsigned int sampleWindow = 50; // Sample window width in milliseconds (50 milliseconds = 20Hz)
    unsigned long endWindow = millis() + sampleWindow;  // End of sample window

    unsigned int signalSample = 0;
    unsigned int signalMin = 4095; // Minimum is the lowest signal below which we assume silence
    unsigned int signalMax = 0; // Maximum signal starts out the same as the Minimum signal

    // collect data for milliseconds equal to sampleWindow
    while (millis() < endWindow)
    {
        signalSample = analogRead(SOUND);
        if (signalSample > signalMax)
        {
            signalMax = signalSample;  // save just the max levels
        }
        else if (signalSample < signalMin)
        {
            signalMin = signalSample;  // save just the min levels
        }
    }

    //SOUNDV = signalMax - signalMin;  // max - min = peak-peak amplitude
    SOUNDV = mapFloat((signalMax - signalMin), 0, 4095, 0, 3.3);



    //return 1;
    return SOUNDV;
}

void readWeatherSi1132()
{

    si1132.begin(); //// initialises Si1132
    Si1132UVIndex = si1132.readUV() *0.01;
    Si1132Visible = si1132.readVisible();
    Si1132InfraRd = si1132.readIR();
}


int MPU9150_readMagSensor(int addrL, int addrH)
{
  Wire.beginTransmission(MPU9150_I2C_ADDRESS);
  Wire.write(addrL);//1
  Wire.endTransmission(false);//0
  Wire.requestFrom(MPU9150_I2C_ADDRESS, 1, true);//1
  byte L = Wire.read();

  Wire.beginTransmission(MPU9150_I2C_ADDRESS);
  Wire.write(addrH);
  Wire.endTransmission(false);

  Wire.requestFrom(MPU9150_I2C_ADDRESS, 1, true);
  byte H = Wire.read();
  return (int16_t)((H<<8)+L);
}


int MPU9150_readSensor(int addr)
{
  Wire.beginTransmission(MPU9150_I2C_ADDRESS);
  Wire.write(addr);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU9150_I2C_ADDRESS, 1, true);
  return Wire.read();
}

int MPU9150_writeSensor(int addr,int data)
{
  Wire.beginTransmission(MPU9150_I2C_ADDRESS);
  Wire.write(addr);
  Wire.write(data);
  return Wire.endTransmission(true);
}

int MPU9150_writeSensor(int addr,uint8_t* data,uint8_t length, bool sendStop)
{
  Wire.beginTransmission(MPU9150_I2C_ADDRESS);
  Wire.write(addr);
  Wire.write(data,length);
  return Wire.endTransmission(sendStop);// Returns 0 on success
}