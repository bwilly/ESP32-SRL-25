#include <Arduino.h>
#include "DHT_SRL.h"
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include "shared_vars.h"
// #include <BufferedLogger.h>

#define DHTTYPE DHT22 // DHT 22 (AM2302)

// Create a DHT object
DHT dht(0, DHTTYPE); // We'll set the pin in the configDHT() function

// Task and synchronization handles
TaskHandle_t sensorTaskHandle = NULL;
SemaphoreHandle_t xSensorDataMutex;

// Shared sensor data variables
float temperature = NAN;
float humidity = NAN;

static int gDhtPin = -1;  // -1 = unset


// Function prototypes [https://chatgpt.com/c/66f37120-74b4-800a-ba48-87c2fc3a22c0]
void configDHT();
float roundToHundredth(float number);
void sensorTask(void *parameter);
void initSensorTask();

void configDHT()
{
    logger.log("configDHT...\n");
    if (gDhtPin < 0) {
        logger.log("DHT: pin not set; cannot configure\n");
        return;
    }

    dht = DHT(gDhtPin, DHTTYPE);
    dht.begin();
    logger.logf("DHT: configured on pin %d\n", gDhtPin);
}


float roundToHundredth(float number)
{
    return round(number * 100.0) / 100.0;
}

void sensorTask(void *parameter)
{
    if (gDhtPin < 0)
    {
        logger.log("DHT pin not configured in sensorTask!\n");
        vTaskDelete(NULL); // Delete the task if pin is not configured
        return;
    }

    configDHT();

    for (;;)
    {
        // Wait a few seconds between measurements
        vTaskDelay(pdMS_TO_TICKS(5000)); // 5 seconds

        // Reading temperature and humidity
        float temp = dht.readTemperature();
        float hum = dht.readHumidity();

        // Check if any reads failed and exit early (to try again).
        if (isnan(temp) || isnan(hum))
        {
            logger.log("Failed to read from DHT sensor! Inside a for(;;) with vTaskDelay(pdMS_TO_TICKS(5000)) \n");
            continue;
        }

        temp = roundToHundredth(temp);
        hum = roundToHundredth(hum);

        // Acquire the mutex before updating the shared variables
        if (xSensorDataMutex != NULL)
        {
            if (xSemaphoreTake(xSensorDataMutex, portMAX_DELAY) == pdTRUE)
            {
                temperature = temp;
                humidity = hum;
                xSemaphoreGive(xSensorDataMutex); // Release the mutex
            }
            else
            {
                logger.log("Failed to take sensor mutex in sensorTask!\n");
            }
        }
        else
        {
            logger.log("⚠️ sensor mutex not initialized in sensorTask!\n");
        }
    }
}

void initSensorTask(int dhtPin)
{
    gDhtPin = dhtPin;

    xSensorDataMutex = xSemaphoreCreateMutex();
    if (xSensorDataMutex == NULL)
    {
        logger.log("Mutex creation failed!\n");
    }

    xTaskCreatePinnedToCore(
        sensorTask,
        "SensorTask",
        4096,
        NULL,
        1,
        &sensorTaskHandle,
        1
    );

    if (sensorTaskHandle == NULL)
    {
        logger.log("Failed to create sensor task!\n");
    }
}


String floatToStringTwoDecimals(float value)
{
    char buffer[10];
    snprintf(buffer, sizeof(buffer), "%.2f", value);
    return String(buffer);
}

float readDHTTemperature()
{
    logger.log("readDHTTemperature...\n");
    float temp = NAN;

    // Acquire the mutex before accessing the shared variable
    if (xSensorDataMutex != NULL)
    {
        if (xSemaphoreTake(xSensorDataMutex, portMAX_DELAY) == pdTRUE)
        {
            temp = temperature;
            xSemaphoreGive(xSensorDataMutex); // Release the mutex
        }
        else
        {
            logger.log("Failed to obtain mutex for temperature!\n");
        }
    }
    else
    {
        logger.log("⚠️ sensor mutex not initialized in readDHTTemperature!\n");
    }

    return temp;
}

float readDHTHumidity()
{
    logger.log("readDHTHumidity...\n");
    float hum = NAN;

    // Acquire the mutex before accessing the shared variable
    if (xSensorDataMutex != NULL)
    {
        if (xSemaphoreTake(xSensorDataMutex, portMAX_DELAY) == pdTRUE)
        {
            hum = humidity;
            xSemaphoreGive(xSensorDataMutex); // Release the mutex
        }
        else
        {
            logger.log("Failed to obtain mutex for humidity!\n");
        }
    }
    else
    {
        logger.log("⚠️ sensor mutex not initialized in readDHTHumidity!\n");
    }

    return hum;
}
