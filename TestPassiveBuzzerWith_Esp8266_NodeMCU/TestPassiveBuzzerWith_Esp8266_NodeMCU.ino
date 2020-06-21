/*
 Name:		TestPassiveBuzzerWith_Esp8266_NodeMCU.ino
 Created:	5/16/2020 11:03:56 PM
 Author:	YLA
*/

// the setup function runs once when you press reset or power the board
void setup() {
    int pin = D0;// 12 = D6 works
    pinMode(pin, OUTPUT);
    pinMode(14, OUTPUT);


    for (;;) {
        tone(pin, 440);
        delay(10000);
        noTone(pin);
        //the tone is emitted correctly
        delay(1000);
    }

    analogWrite(14, 200);
    delay(1000);
    analogWrite(14, 0);

    tone(pin, 440);
    delay(1000);
    noTone(pin);
}

// the loop function runs over and over again until power down or reset
void loop() {
  
}
