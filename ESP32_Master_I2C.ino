#include <Wire.h>

void setup() {
 Serial.begin(9600); /* begin serial for debug */
 Wire.begin(16, 17); /* join i2c bus with SDA=D16 and SCL=D17 of NodeMCU 32S */
 // on Arduino Uno, SDA=A4, and SCL=A5
}

void loop() {
 Wire.beginTransmission(8); /* begin with device address 8 */
 //Wire.write("Hello Arduino");  /* sends hello string */
 Wire.endTransmission();    /* stop transmitting */

 Wire.requestFrom(8, 1); /* request & read data of size 1 from slave */
 while(Wire.available()){
    int c = Wire.read();
  Serial.print(c);
 }
 Serial.println();
 delay(1000);
}
