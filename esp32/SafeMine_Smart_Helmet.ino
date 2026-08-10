#include <Wire.h>
#include <DHT.h>
#include <MPU6050.h>
#include <Adafruit_BMP085.h>

// ================= PIN CONFIGURATION =================

#define DHTPIN 4
#define DHTTYPE DHT11

#define MQ2_PIN 34
#define BUZZER_PIN 25

// ================= SAFETY THRESHOLDS =================

#define TEMP_THRESHOLD 40
#define GAS_THRESHOLD 1200

// 10 minutes = 600000 milliseconds
#define NO_MOTION_TIME 600000UL

// ================= SENSOR OBJECTS =================

DHT dht(DHTPIN, DHTTYPE);
MPU6050 mpu;
Adafruit_BMP085 bmp;

// ================= VARIABLES =================

unsigned long lastMotionTime = 0;

int16_t ax = 0;
int16_t ay = 0;
int16_t az = 0;

int16_t prevAx = 0;
int16_t prevAy = 0;
int16_t prevAz = 0;

bool firstMotionReading = true;

bool gasAlert = false;
bool tempAlert = false;
bool motionAlert = false;

bool bmpOK = false;
bool mpuOK = false;

// ================= BUZZER FUNCTION =================

void beepBuzzer()
{
  Serial.println("BUZZER ON");

  digitalWrite(BUZZER_PIN, HIGH);
  delay(2000);
  digitalWrite(BUZZER_PIN, LOW);

  Serial.println("BUZZER OFF");
}

// ================= SETUP =================

void setup()
{
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("=================================");
  Serial.println("       SAFEMINE SMART HELMET");
  Serial.println("       ESP32 SENSOR SYSTEM");
  Serial.println("=================================");
  Serial.println();

  // Buzzer
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // MQ2
  pinMode(MQ2_PIN, INPUT);

  // I2C
  Wire.begin(21, 22);

  // DHT11
  dht.begin();
  Serial.println("DHT11 initialized");

  // BMP180
  if (bmp.begin())
  {
    bmpOK = true;
    Serial.println("BMP180 OK");
  }
  else
  {
    bmpOK = false;
    Serial.println("BMP180 ERROR");
  }

  // MPU6050
  mpu.initialize();

  if (mpu.testConnection())
  {
    mpuOK = true;
    Serial.println("MPU6050 OK");
  }
  else
  {
    mpuOK = false;
    Serial.println("MPU6050 ERROR");
  }

  // Start motion timer
  lastMotionTime = millis();

  Serial.println();
  Serial.println("System initialization complete.");
  Serial.println("Starting sensor readings...");
  Serial.println();

  delay(2000);
}

// ================= MAIN LOOP =================

void loop()
{
  // -------------------------------------------------
  // READ DHT11
  // -------------------------------------------------

  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  // -------------------------------------------------
  // READ MQ2
  // -------------------------------------------------

  int gas = analogRead(MQ2_PIN);

  // -------------------------------------------------
  // READ BMP180
  // -------------------------------------------------

  float pressure = 0;

  if (bmpOK)
  {
    pressure = bmp.readPressure() / 100.0;
  }

  // -------------------------------------------------
  // READ MPU6050
  // -------------------------------------------------

  if (mpuOK)
  {
    mpu.getAcceleration(&ax, &ay, &az);
  }

  // =================================================
  // SERIAL MONITOR
  // =================================================

  Serial.println("----------------------------------");

  // Temperature
  Serial.print("Temperature : ");

  if (isnan(temperature))
  {
    Serial.println("Sensor Error");
  }
  else
  {
    Serial.print(temperature);
    Serial.println(" C");
  }

  // Humidity
  Serial.print("Humidity    : ");

  if (isnan(humidity))
  {
    Serial.println("Sensor Error");
  }
  else
  {
    Serial.print(humidity);
    Serial.println(" %");
  }

  // Gas
  Serial.print("Gas Value   : ");
  Serial.println(gas);

  // Pressure
  Serial.print("Pressure    : ");

  if (bmpOK)
  {
    Serial.print(pressure);
    Serial.println(" hPa");
  }
  else
  {
    Serial.println("Sensor Error");
  }

  // MPU
  Serial.print("AX          : ");
  Serial.println(ax);

  Serial.print("AY          : ");
  Serial.println(ay);

  Serial.print("AZ          : ");
  Serial.println(az);

  // =================================================
  // MOTION DETECTION
  // =================================================

  if (mpuOK)
  {
    if (firstMotionReading)
    {
      // First reading ko baseline maan lenge
      prevAx = ax;
      prevAy = ay;
      prevAz = az;

      firstMotionReading = false;
      lastMotionTime = millis();

      Serial.println("Motion baseline initialized.");
    }
    else
    {
      bool motionDetected =
          abs(ax - prevAx) > 500 ||
          abs(ay - prevAy) > 500 ||
          abs(az - prevAz) > 500;

      if (motionDetected)
      {
        Serial.println("Motion detected.");

        lastMotionTime = millis();
        motionAlert = false;
      }

      // Current values ko previous values bana do
      prevAx = ax;
      prevAy = ay;
      prevAz = az;
    }
  }

  // =================================================
  // GAS ALERT
  // =================================================

  if (gas >= GAS_THRESHOLD && !gasAlert)
  {
    Serial.println();
    Serial.println(">>> GAS ALERT <<<");
    Serial.print("Gas value exceeded limit: ");
    Serial.println(gas);

    beepBuzzer();

    gasAlert = true;
  }

  // Gas normal hone par alert reset
  if (gas < (GAS_THRESHOLD - 100))
  {
    gasAlert = false;
  }

  // =================================================
  // TEMPERATURE ALERT
  // =================================================

  if (!isnan(temperature) &&
      temperature >= TEMP_THRESHOLD &&
      !tempAlert)
  {
    Serial.println();
    Serial.println(">>> HIGH TEMPERATURE <<<");
    Serial.print("Temperature: ");
    Serial.print(temperature);
    Serial.println(" C");

    beepBuzzer();

    tempAlert = true;
  }

  // Temperature normal hone par alert reset
  if (!isnan(temperature) &&
      temperature < (TEMP_THRESHOLD - 2))
  {
    tempAlert = false;
  }

  // =================================================
  // NO MOTION ALERT
  // =================================================

  if (mpuOK &&
      !firstMotionReading &&
      (millis() - lastMotionTime >= NO_MOTION_TIME) &&
      !motionAlert)
  {
    Serial.println();
    Serial.println(">>> NO MOTION FOR 10 MINUTES <<<");
    Serial.println("Worker may require assistance.");

    beepBuzzer();

    motionAlert = true;
  }

  // =================================================
  // SYSTEM STATUS
  // =================================================

  Serial.println();
  Serial.print("System Status : ");

  if (bmpOK && mpuOK)
  {
    Serial.println("ONLINE");
  }
  else
  {
    Serial.println("SENSOR ERROR");
  }

  Serial.println("----------------------------------");

  // Read sensors every 1 second
  delay(1000);
}