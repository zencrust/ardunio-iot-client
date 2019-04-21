#include <Wire.h>

const int watermeterPin = 2;

volatile int  pulse_frequency;
int  literpermin;
unsigned long currentTime, loopTime;
byte sensorInterrupt = 0;

void setup()
{ 
   pinMode(watermeterPin, INPUT);
   Wire.begin(8);                /* join i2c bus with address 8 */
//    Wire.onReceive(receiveEvent); /* register receive event */
    Wire.onRequest(requestEvent); /* register request event */

   Serial.begin(9600); 
   attachInterrupt(sensorInterrupt, getFlow, FALLING);
                                     
   currentTime = millis();
   loopTime = currentTime;
} 

void loop ()    
{
   currentTime = millis();
   if(currentTime >= (loopTime + 1000))
   {
      loopTime = currentTime;
      literpermin = (pulse_frequency / 7.5);
      pulse_frequency = 0;
      Serial.print(literpermin, DEC);
      Serial.println(" Liter/min");
   }
}
void getFlow ()
{ 
   pulse_frequency++;
} 

// this function executes whenever data is requested from ESP32 Master
void requestEvent() {
 Wire.write(literpermin);  /*send string on request */
 // refer https://www.arduino.cc/en/Reference/WireWrite for more info
}
