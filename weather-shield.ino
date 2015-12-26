// This #include statement was automatically added by the Particle IDE.
#include "SparkFun_MPL3115A2.h"

// This #include statement was automatically added by the Particle IDE.
#include "SparkFunPhant.h"

// This #include statement was automatically added by the Particle IDE.
#include "HTU21D.h"

// This #include statement was automatically added by the Particle IDE.
#include "elapsedMillis/elapsedMillis.h"

#include "math.h""

elapsedMillis loopElapsed; //declare global if you don't want it reset every time loop runs
unsigned int INTERVAL = 30000;

elapsedMillis phantElapsed;
const unsigned int MAX_PHANT_INTERVAL = 300000;

// data.sparkfun.com Phant cloud
const char server[] = "data.sparkfun.com"; // Phant destination server
const char publicKey[] = ""; // Phant public key
const char privateKey[] = ""; // Phant private key
//Phant phant(server, publicKey, privateKey); // Create a Phant object
int airSensorPin = D3;

const char publicKeyAir[] = ""; // Phant public key
const char privateKeyAir[] = ""; // Phant private key


HTU21D htu = HTU21D();//create instance of HTU21D Temp and humidity sensor
MPL3115A2 baro = MPL3115A2();//create instance of MPL3115A2 barrometric sensor

typedef struct 
{
    float humidity;
    int humidityErrorCount;
    
    float tempf;
    int tempfErrorCount;
    
    float pascals;
    int pascalsErrorCount;
    
    float baroTemp;
    int baroTempErrorCount;
    
    
    float ratio;
    float concentration;
    unsigned long lowpulseoccupancy;

    //float lightning;
    
    int reboot;
} Readings;
Readings readings {-100, 0, -100, 0, -100, 0, -100, 0, 1, 0, 0, 0};

//---------------------------------------------------------------
int postToPhant()
{
    Phant phant(server, publicKey, privateKey); // Create a Phant object
    if(readings.humidity == -100 || readings.tempf == -100 || readings.pascals == -100 || readings.baroTemp == -100)
        return -4;
        
    phant.add("humidity", readings.humidity);
    phant.add("pressure", readings.pascals);
    phant.add("temperature", (readings.tempf + readings.baroTemp) / 2);
    //phant.add("lightning", "");
    phant.add("reboot", readings.reboot);

    
    TCPClient client;
    char response[512];
    int i = 0;
    int retVal = 0;

    if (client.connect(server, 80))
    {
        Serial.println("Posting!");
        client.print(phant.post());
        delay(1000);
        while (client.available())
        {
            char c = client.read();
            if (i < 512)
                response[i++] = c;
        }
        if (strstr(response, "200 OK"))
        {
            Serial.println("Post success!");
            retVal = 1;
            phantElapsed = 0;
            readings.reboot = 0;
        }
        else if (strstr(response, "400 Bad Request"))
        {
            Serial.println("Bad request");
            retVal = -1;
        }
        else
        {
            retVal = -2;
        }
    }
    else
    {
        Serial.println("connection failed");
        retVal = -3;
    }
    client.stop();
    return retVal;

}

int postToPhantAirQuality()
{
    if(readings.lowpulseoccupancy == 0 || readings.ratio == 0)
        return -1;

    Phant phant(server, publicKeyAir, privateKeyAir); // Create a Phant object
    phant.add("lpo", readings.lowpulseoccupancy);
    phant.add("ratio", readings.ratio);
    phant.add("concentration", readings.concentration);

    readings.lowpulseoccupancy = 0;     //reset

    TCPClient client;
    char response[512];
    int j = 0;
    int retVal = 0;
    
    if (client.connect(server, 80)) // Connect to the server
    {
        Serial.println("Air Quality posting!"); 

        client.print(phant.post());
        delay(1000);
        while (client.available())
        {
            char c = client.read();
            //Serial.print(c);	// Print the response for debugging help.
            if (j < 512)
                response[j++] = c; // Add character to response string
        }
        if (strstr(response, "200 OK"))
        {
            Serial.println("Air Quality post success!");
            retVal = 1;
        }
        else if (strstr(response, "Air Quality 400 Bad Request"))
        {
            Serial.println("Air Quality bad request");
            retVal = -1;
        }
        else
        {
            retVal = -2;
        }
    }
    else
    {	// If the connection failed, print a message:
        Serial.println("Air Quality connection failed");
        retVal = -3;
    }
    client.stop();	// Close the connection to server.
    return retVal;	// Return error (or success) code.
}


//---------------------------------------------------------------
void printInfo()
{
      Serial.print("Temp: ");
      Serial.print(readings.tempf);
      Serial.print(" ");
      Serial.print(readings.baroTemp);
      Serial.print(" (");
      Serial.print((readings.tempf + readings.baroTemp)/2);
      Serial.print(")F  ");

      Serial.print("Humidity: ");
      Serial.print(readings.humidity);
      Serial.print("%  ");

      Serial.print("Pressure:");
      Serial.print(readings.pascals);
      Serial.print("Pa  ");

      //Serial.print("Altitude:");
      //Serial.print(altf);
      //Serial.print("ft  ");
      
      Serial.println("");
      
      
      Serial.print("Error counts: ");
      Serial.print(readings.tempfErrorCount);
      Serial.print("  ");
      Serial.print(readings.baroTempErrorCount);
      Serial.print("  ");
      Serial.print(readings.humidityErrorCount);
      Serial.print("  ");
      Serial.print(readings.pascalsErrorCount);
      Serial.println("");
      
      Serial.print("Air Quality: ");
      Serial.print(readings.concentration);
      Serial.print("  ");
      Serial.print(readings.lowpulseoccupancy);
      Serial.print("  ");
      Serial.print(readings.ratio);
      Serial.println("");
}
//---------------------------------------------------------------

void getAirQuality()
{
    unsigned long duration = pulseIn(airSensorPin, LOW);
    readings.lowpulseoccupancy = readings.lowpulseoccupancy + duration;
    readings.ratio = readings.lowpulseoccupancy / (INTERVAL * 10.0);    // Integer percentage 0=>100
    readings.concentration = 1.1*pow(readings.ratio,3)-3.8*pow(readings.ratio,2)+520*readings.ratio+0.62; // using spec sheet curve
}


//---------------------------------------------------------------
void getTempHumidity()
{
    float temp = (htu.readTemperature() * 1.8) + 32.;
    if(temp <= 150 && temp >= -50)
    {
        readings.tempf = temp;
        readings.tempfErrorCount = 0;
    }
    else
    {
        readings.tempfErrorCount++;
    }
    
    float humidityTemp = htu.readHumidity();
    if(humidityTemp <= 100 && humidityTemp > 0)
    {
        readings.humidity = humidityTemp;
        readings.humidityErrorCount = 0;
    }
    else
    {
        readings.humidityErrorCount++;
    }
}
//---------------------------------------------------------------
void getBaro()
{
    float temp = baro.readTempF();
    if(temp <= 150 && temp >= -50)
    {
        readings.baroTemp = temp;
        readings.baroTempErrorCount = 0;
    }
    else
    {
        readings.baroTempErrorCount++;
    }

    float pascalsTemp = baro.readPressure();
    if(pascalsTemp < 109000 && pascalsTemp > 94000)
    {
        readings.pascals = pascalsTemp;
        readings.pascalsErrorCount = 0;
    }
    else
    {
        readings.pascalsErrorCount++;
    }
    //pascals = baro.readPressure();//get pressure in Pascals
    //altf = baro.readAltitudeFt();//get altitude in feet
}
//---------------------------------------------------------------

void calcWeather()
{
    getTempHumidity();
    getBaro();
    getAirQuality();
}


void setup()
{
    Serial.begin(9600);   // open serial over USB

    while(! htu.begin())
    {
        Serial.println("HTU21D not found");
        delay(500);
    }
    Serial.println("HTU21D OK");
    
	while(! baro.begin())
    {
        Serial.println("MPL3115A2 not found");
        delay(500);
    }
    Serial.println("MPL3115A2 OK");

    //MPL3115A2 Settings
    baro.setModeBarometer();//Set to Barometer Mode
    //baro.setModeAltimeter();//Set to altimeter Mode
    baro.setOversampleRate(7); // Set Oversample to the recommended 128
    baro.enableEventFlags(); //Necessary register calls to enble temp, baro ansd alt

    //RGB.control(true);  // take control of the RGB LED
    //RGB.brightness(0);  // turn it off

    //Take first reading before heading into loop()
    calcWeather();
    printInfo();
    postToPhant();
    postToPhantAirQuality();
}


void loop()
{
    if (loopElapsed > INTERVAL) 
    {
        loopElapsed = 0;              // reset the counter to 0 so the counting starts over...
        calcWeather();
        printInfo();
        
        
        if( readings.humidityErrorCount >= 3 || readings.tempfErrorCount >= 3 || readings.pascalsErrorCount >= 3 || readings.baroTempErrorCount >= 3)
            System.reset();

        postToPhant();
        postToPhantAirQuality();
    }

    if (phantElapsed > MAX_PHANT_INTERVAL)      // no successful phant update in MAX_PHANT_INTERVAL, reset.  phantElapsed is updated in postToPhant()
        System.reset();
    
    delay(1000);
}
