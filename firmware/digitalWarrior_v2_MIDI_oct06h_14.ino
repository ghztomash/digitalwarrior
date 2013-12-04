// digital warrior by Tomash Ghz
// http://digitalwarrior.co/
// http://tomashg.com/
//
// licenced under Creative Commons Attribution-ShareAlike 4.0
// http://creativecommons.org/licenses/by-sa/4.0/

// firmware version 1.4

#include <Wire.h>
#include "Adafruit_MCP23017.h"
#include "Adafruit_MCP23008.h"
#include <MIDIElements.h>
#include <Encoder.h>
#include <EEPROM.h>
#include <Bounce.h>

// Butonpad Variables
Adafruit_MCP23008 _buttonpad;
int buttonRow[]= {
  5, 4, 7, 6
};
int buttonCol[]= {
  0, 1, 2, 3
};
int buttons[4][4];
unsigned long buttonsBounce[4][4];

// Display Variables
Adafruit_MCP23017 _display;
int dispGroundPins[4] = {
  13, 12, 15, 14
}; // ground pins - columns
int dispRedPins[4] = {
  0, 3, 6, 9
}; // red pins - rows
int dispGreenPins[4] = {
  1, 4, 7, 10
}; // reen pins - rows
int dispBluePins[4] = {
  2, 5, 8, 11
}; // blue pins - rows
int displayBuffer[4][4]; // store the values that are displayed

// display pages
int displayPages[8][4][4];

int bounceDelay=24;//26 //30

Bounce pageShift = Bounce(0, 30); // page shift button
int page=0;
boolean pageSelect=false;

// colors
const int OFF = 0;
const int RED = 1;
const int GREEN = 2;
const int YELLOW = 3;
const int BLUE = 4;
const int PINK = 5;
const int CYAN = 6;
const int WHITE = 7;

int pageColor[8]={GREEN,YELLOW,BLUE,RED,GREEN,GREEN,GREEN,GREEN};   //*******

int midiChannel=1; // midi channel number   ******
int controlChannel=2; // midi channel number    *******
int sequencerChannel=3; // midi channel number  *******
boolean debug=false; // print to serial instead of midi
boolean secondary=false; // enable secondary midi messages  ***********
boolean abletonEncoder=false;     //      *******
int encodersBanked=1;  // *********

Potentiometer *pot1;//(14, controlChannel, 17, false, debug); // knob on pin 45 (A7)
Potentiometer *pot2;//(15, controlChannel, 18, false, debug); // knob on pin 44 (A6)
Potentiometer *pot3;//(16, controlChannel, 19, false, debug); // knob on pin 45 (A7)
Potentiometer *pot4;//(17, controlChannel, 20, false, debug); // knob on pin 44 (A6)

Button *encBut[8][2];
RGLed *encLed[8][2];

Encoder encA(8, 7);
Encoder encB(10, 9);

unsigned long int encNewA[8] = {
  0, 0, 0, 0, 0, 0, 0, 0
};
unsigned long int encOldA[8] = {
  999, 999, 999, 999, 999, 999, 999, 999
};

unsigned long int encNewB[8] = {
  0, 0, 0, 0, 0, 0, 0, 0
};
unsigned long int encOldB[8] = {
  999, 999, 999, 999, 999, 999, 999, 999
};

int encoderLedValueA[8]= {
  0, 0, 0, 0, 0, 0, 0, 0
}; //store the dispalyed LED values
int encoderLedValueB[8]= {
  0, 0, 0, 0, 0, 0, 0, 0
};

float encLedOffset=4;

int displayEnc=0; // 0 none, 1 enc A, 2 enc b, 3 both
unsigned long displayTimer=0;

// step sequencer variables

boolean stepSequencer=true;  // ****
boolean sequencerPaused=false;
int selectedChannel=0;
int currentStep=0;
int STEPS=32; //lenth of steps ****
int selectedPattern[16]; // sellected voice pattern
boolean stepState[4][16][32];
boolean lastPlayed[16];
boolean hasPattern[16];
boolean muted[16];
int counter=0;

byte clockCounter=0;
byte CLOCK=248;
byte START = 250; 
byte CONTINUE = 251; 
byte STOP = 252; 

boolean setupMode=false;
int setupPage=0;

// #####################################################################################################################
void setup() {
  pinMode(0, INPUT_PULLUP); //shift button

  _display.begin(1);  // use default address 1
  _buttonpad.begin(0);      // use default address 0

  initButtonpad();
  initDisplay();      // crlear display
  clearPages();

  // init sequencer
  for (int i=0; i<16; i++) {
    lastPlayed[i]=false;
    hasPattern[i]=false;
    muted[i]=false;
    selectedPattern[i]=0;
    buttonsBounce[i%4][i/4]=0;
  }
  loadSetup();
  welcomeAnimation();
  
  for (int i=0; i<8; i++) {
    encBut[i][0]=new Button(1, controlChannel, 0+i*2, false, debug); // encoder buttons
    encBut[i][1]=new Button(2, controlChannel, 1+i*2, false, debug);
    encLed[i][0]=new RGLed(4, 3, controlChannel, 0+i*2, false);//encoder LED
    encLed[i][1]=new RGLed(6, 5, controlChannel, 1+i*2, false);//encoder LED
  }
  
  pot1=new Potentiometer(14, controlChannel, 17, false, debug); // knob on pin 45 (A7)
  pot2=new Potentiometer(15, controlChannel, 18, false, debug); // knob on pin 44 (A6)
  pot3=new Potentiometer(16, controlChannel, 19, false, debug); // knob on pin 45 (A7)
  pot4=new Potentiometer(17, controlChannel, 20, false, debug); // knob on pin 44 (A6)
  
  if(stepSequencer)
    setPagePixel(WHITE, 5, selectedChannel);
  
  usbMIDI.setHandleNoteOff(OnNoteOff); //set event handler for note off
  usbMIDI.setHandleNoteOn(OnNoteOn); //set event handler for note on
  usbMIDI.setHandleControlChange(OnCC); // set event handler for CC
  usbMIDI.setHandleRealTimeSystem(RealTimeSystem); // step sequencer
  
  attachInterrupt(10, readEncoderB, CHANGE);
  attachInterrupt(9, readEncoderB, CHANGE);
  attachInterrupt(8, readEncoderA, CHANGE);
  attachInterrupt(7, readEncoderA, CHANGE);
}

// #####################################################################################################################
void loop() {

  // read page shift buttons
  if ( pageShift.update() ) {
    if ( pageShift.read() == LOW) {
      pageSelect=true;
      //Serial.println("page ");
      //displayEnc=0; // stop displaying encoders
      //saveLedState();
    }
    else
    {
      pageSelect=false;
      //displayEnc=0; // stop displaying encoders
      //loadLedState();
    }
  }

  if (pageSelect) {
    changePage();
    printBuffer(displayBuffer);
    displayEnc=0; // stop displaying encoders
  }
  else if (displayEnc!=0) {
    if (millis()-displayTimer>200) { // display for a second
      displayEnc=0;
    }
    else {

      switch(displayEnc) {
      case 3:  // show both 
        setLedTo(encoderLedValueA[page*encodersBanked], encoderLedValueB[page*encodersBanked]);
        break;
      case 2:  // show B
        setLedToA(encoderLedValueA[page*encodersBanked]);
        break;
      case 1:  // show A
        setLedToB(encoderLedValueB[page*encodersBanked]);
        break;
      }

      printBuffer(displayBuffer);
      readButtonsGPIO();
    }
  }
  else {

    if ((stepSequencer)&&((page==5))) {
      for (int i=0; i<16; i++){
        //hasPattern[i]=false;
        for (int j=0; j<STEPS; j++) {
          if (stepState[selectedPattern[i]][i][j]){
            //hasPattern[i]=true;
            if(muted[i]){
              setPagePixel(RED, 5, i);
             }
            else
              setPagePixel(YELLOW, 5, i);
          }
        }
      }
      setPagePixel(WHITE, 5, selectedChannel);
    }
    else if ((stepSequencer)&&((page==6))) {


      for (int i=0; i<STEPS; i++) {
        if ((stepState[selectedPattern[selectedChannel]][selectedChannel][i])&&(i<16)) {
          setPagePixel(GREEN, 6, i%16);
        }
        else if (i<16) {
          setPagePixel(0, 6, i%16);
        }
      }

      if (currentStep<16) {
        setPagePixel(BLUE, 6, currentStep%16);
      }
    }
    else if ((stepSequencer)&&((page==7))) {
      for (int i=0; i<STEPS; i++) {      
        if ((stepState[selectedPattern[selectedChannel]][selectedChannel][i])&&(i>15)) {
          setPagePixel(GREEN, 7, i%16);
        }
        else if (i>15) {
          setPagePixel(0, 7, i%16);
        }
      }
      if (currentStep>15) {
        setPagePixel(BLUE, 7, currentStep%16);
      }
    }

    printBuffer(displayPages[page]);
    readButtonsGPIO();
  }

  pot1->read();
  pot2->read();
  pot3->read();
  pot4->read();

  encBut[page*encodersBanked][0]->read();
  encBut[page*encodersBanked][1]->read();

  encLed[page*encodersBanked][0]->set();
  encLed[page*encodersBanked][1]->set();

  //readEncoders();

  usbMIDI.read(); // read all the incoming midi messages
}

// #####################################################################################################################
void initButtonpad() {
  for (int i=0; i<4; i++) {
    _buttonpad.pinMode(buttonRow[i], OUTPUT);
    _buttonpad.digitalWrite(buttonRow[i], HIGH);
    _buttonpad.pinMode(buttonCol[i], INPUT);
    _buttonpad.pullUp(buttonCol[i], HIGH);  // turn on a 100K pullup internally
  }
  for (int row=0; row<4; row++) {
    for (int col =0; col<4; col++) {
      buttons[row][col]=pow(2, col);
    }
  }
}

// #####################################################################################################################
// set display pins to initial state
void initDisplay() {
  for ( int i=0; i<4; i++) {
    _display.pinMode(dispGroundPins[i], OUTPUT);
    _display.pinMode(dispRedPins[i], OUTPUT);
    _display.pinMode(dispGreenPins[i], OUTPUT);
    _display.pinMode(dispBluePins[i], OUTPUT);

    _display.digitalWrite(dispGroundPins[i], HIGH);
    _display.digitalWrite(dispRedPins[i], LOW);
    _display.digitalWrite(dispGreenPins[i], LOW);
    _display.digitalWrite(dispBluePins[i], LOW);
  }
}

// #####################################################################################################################
// clear the pages array

void clearPages() {
  for (int p=0; p<8; p++) {
    for (int col=0; col <4; col++) {
      for (int row=0; row <4; row++) {
        displayPages[p][col][row]=0;
      }
    }
  }
}

// #####################################################################################################################
//set the specific pixel in the buffer to a specified color
void setPagePixel(int rgb, int p, int row, int col) {
  displayPages[p][col][row]=rgb%8;
}

void setPagePixel(int rgb, int p, int pixel) {
  int col = pixel % 4;
  int row = pixel / 4;
  displayPages[p][col][row]=rgb%8;
}

// #####################################################################################################################
//set the specific pixel in the buffer to a specified color
void setBufferPixel(int rgb, int row, int col) {
  displayBuffer[col][row]=rgb%8;
}

void setBufferPixel(int rgb, int pixel) {
  int col = pixel % 4;
  int row = pixel / 4;

  displayBuffer[col][row]=rgb%8;
}

// #####################################################################################################################
void setLedToA(int num) {
  num=map(num, 0, 127, 0, 16);
  constrain(num, 0, 15);
  clearBuffer();
  for (int i=0;i<num; i++) {
    setBufferPixel(BLUE, i);
  }
}
void setLedToB(int num) {
  num=map(num, 0, 127, 0, 16);
  constrain(num, 0, 15);
  clearBuffer();
  for (int i=0;i<num; i++) {
    setBufferPixel(GREEN, i);
  }
}

void setLedTo(int numa, int numb) {
  numa=map(numa, 0, 127, 0, 8);
  numb=map(numb, 0, 127, 0, 8);
  constrain(numa, 0, 7);
  constrain(numb, 0, 7);
  clearBuffer();
  for (int i=0; i<numa ; i++) {
    setBufferPixel(BLUE, i);
  }
  for (int i=0; i<numb ; i++) {
    setBufferPixel(GREEN, i+8);
  }
}

// #####################################################################################################################
void clearBuffer() {
  for (int i=0;i<4; i++)
    for (int j=0; j<4; j++)
      displayBuffer[i][j]=0;
}

// #####################################################################################################################
// function to print buffer on the display
void printBuffer(int pb[4][4]) {
  noInterrupts();
  int counter; 
  //_display.writeGPIOAB(61440);
  for (int col=0; col<4; col++) {
    counter = 61440 - (1<<dispGroundPins[col]);
    _display.writeGPIOAB(61440);

    for (int row=0; row<4; row++) {

      if ((pb[col][row]&(1<<0)) != 0) { // chech if theres red
        counter+=1<<dispRedPins[row];
      }
      if ((pb[col][row]&(1<<1)) != 0) { // chech if theres green
        counter+=1<<dispGreenPins[row];
      }
      if ((pb[col][row]&(1<<2)) != 0) { // chech if theres blue
        counter+=1<<dispBluePins[row];
      }
    }
    _display.writeGPIOAB(counter);
  }
  _display.writeGPIOAB(61440);
  interrupts();
}

// #####################################################################################################################
// function to print buffer on the display
void printBuffer() {
  noInterrupts();
  int counter; 
  //_display.writeGPIOAB(61440);
  for (int col=0; col<4; col++) {
    counter = 61440 - (1<<dispGroundPins[col]);
    _display.writeGPIOAB(61440);

    for (int row=0; row<4; row++) {

      if ((displayBuffer[col][row]&(1<<0)) != 0) { // chech if theres red
        counter+=1<<dispRedPins[row];
      }
      if ((displayBuffer[col][row]&(1<<1)) != 0) { // chech if theres green
        counter+=1<<dispGreenPins[row];
      }
      if ((displayBuffer[col][row]&(1<<2)) != 0) { // chech if theres blue
        counter+=1<<dispBluePins[row];
      }
    }
    _display.writeGPIOAB(counter);
  }
  _display.writeGPIOAB(61440);
  interrupts();
}

// #####################################################################################################################
void welcomeAnimation() {

  for ( int i=1; i<200; i++) {
    if (random(10)<5) {
      setBufferPixel(random(0, 7), random(0, 4), random(0, 4));
    }
    if (random(20)<2) {
      analogWrite(3, random(0, 64));
      analogWrite(4, random(0, 64));
      analogWrite(5, random(0, 64));
      analogWrite(6, random(0, 64));
    }
    if ( pageShift.update() ) {
    if ( pageShift.read() == LOW) {
      setupMode=true;
    }
    }
    delay(1);
    printBuffer();
  }
  clearBuffer();
  analogWrite(3, 0);
  analogWrite(4, 0);
  analogWrite(5, 0);
  analogWrite(6, 0);
  
  if(setupMode)
    setupMenu();
}

// #####################################################################################################################
void readButtonsGPIO() {
  noInterrupts();
  int state;
  int bState;

  //row
  for (int row=0; row<4; row++) {
    _buttonpad.writeGPIO(240-(1<<buttonRow[row]));
    state = _buttonpad.readGPIO();    
    for (int col =0; col<4; col++) {

      bState=state&(1<<col);
      
      if(bState!=buttons[row][col]){
        
        if ((millis() - buttonsBounce[row][col])>bounceDelay){

          if (bState==0) {
            buttonsBounce[row][col]=millis();
            displayEnc=0; // stop displaying encoders
            //Serial.print((4*row+col)+(col*3-row*3));
            //Serial.println(" pressed");
            if ((stepSequencer)&&(page==5)) // sellect a voice
            {
              setPagePixel(0, page, selectedChannel);
              selectedChannel=4*col+row;
              setPagePixel(WHITE, page, selectedChannel);
            }
            else if ((stepSequencer)&&(page==6)) // enter pattern
            {
              stepState[selectedPattern[selectedChannel]][selectedChannel][4*col+row]=!stepState[selectedPattern[selectedChannel]][selectedChannel][4*col+row];
            }
            else if ((stepSequencer)&&(page==7)&&(STEPS==32)) { // enter pattern second
              stepState[selectedPattern[selectedChannel]][selectedChannel][4*col+row+16]=!stepState[selectedPattern[selectedChannel]][selectedChannel][4*col+row+16];
            }
            else
              midiNoteOnOff(true, (4*row+col)+(col*3-row*3)+page*16);
            //setBufferPixel(random(1,7),col,row);
          }
          else if (bState!=0) {
            //Serial.print((4*row+col)+(col*3-row*3));
            //Serial.println(" released");
            if ((stepSequencer)&&((page==5)||(page==6)||((page==7)&&(STEPS==32)))) {
            }
            else
              midiNoteOnOff(false, (4*row+col)+(col*3-row*3)+page*16);
            //setBufferPixel(0,col,row);
          }
          buttons[row][col] = bState;
        }
      }
    }
  }
  interrupts();
}

// #####################################################################################################################
// function to handle noteon outgoing messages
void midiNoteOnOff(boolean s, int n) {

  if (s) {
    if (debug) {//debbuging enabled
      Serial.print("Button ");
      Serial.print(n);
      Serial.println(" pressed.");
    }
    else {
      usbMIDI.sendNoteOn(n, 127, midiChannel);
      if(secondary)
        usbMIDI.sendControlChange(n, 127, midiChannel);
    }
  }
  else {
    if (debug) {//debbuging enabled
      Serial.print("Button ");
      Serial.print(n);
      Serial.println(" released.");
    }
    else {
      usbMIDI.sendNoteOff(n, 0, midiChannel);
      if(secondary)
        usbMIDI.sendControlChange(n, 0, midiChannel);
    }
  }
}

// #####################################################################################################################
// event handlers
void OnNoteOn(byte channel, byte note, byte velocity) {
  // add all your output component sets that will trigger with note ons
  if(encodersBanked==1){
    for (int i=0; i<8;i++) {
      encLed[i][0]->setOnSilent(channel, note, velocity);
      encLed[i][1]->setOnSilent(channel, note, velocity);
    }
  }else
  {
   encLed[0][0]->setOnSilent(channel, note, velocity);
   encLed[0][1]->setOnSilent(channel, note, velocity);
  }

  if (channel==midiChannel) {

    if ((note<=127)&&(note>=0))
    {

      int ld=note%16; // find the led
      int pg=note/16; // find the page

      if (velocity!=0) {
        if(velocity==127){
          velocity=pageColor[pg]-1;
        }
        else if ((velocity==1)||(velocity==126)) {
          velocity=GREEN-1;
        }
        else if ((velocity==2)||(velocity==125)) {
          velocity=YELLOW-1;
        }
        else if ((velocity==3)||(velocity==124)) {
          velocity=RED-1;
        }
        setPagePixel((velocity%7)+1, pg, ld);
        //setPagePixel(BLUE,pg,ld);
      }
      else
        setPagePixel(0, pg, ld);
    }
  }
}

// #####################################################################################################################
void OnNoteOff(byte channel, byte note, byte velocity) {
  // add all your output component sets that will trigger with note ons
  if(encodersBanked==1){
    for (int i=0; i<8;i++) {
      encLed[i][0]->setOff(channel, note, velocity);
      encLed[i][1]->setOff(channel, note, velocity);
    }
  }else{
    encLed[0][0]->setOff(channel, note, velocity);
    encLed[0][1]->setOff(channel, note, velocity);
  }

  if (channel==midiChannel) {
    if ((note<=127)&&(note>=0))
    {
      int ld=note%16; // find the led
      int pg=note/16; // find the page
      setPagePixel(0, pg, ld);
    }
  }
}

// #####################################################################################################################
void readEncoderB() {
  noInterrupts();
  encNewB[page*encodersBanked] = encB.read();
  //encoderLedValueB[page] = constrain(newPositionB[page],0,127);

  if (encNewB[page*encodersBanked] != encOldB[page*encodersBanked]) {

    if (encNewB[page*encodersBanked]>encOldB[page*encodersBanked]) {
      if (debug) {
        Serial.print(" encoder ");
        Serial.print(1);
        Serial.println(" >> ");
      }
      else {   
        if (abletonEncoder) {
          usbMIDI.sendControlChange(1+page*2*encodersBanked, 70, controlChannel);
          encoderLedValueB[page*encodersBanked]+=encLedOffset;
          encoderLedValueB[page*encodersBanked] = constrain(encoderLedValueB[page*encodersBanked], 0, 127);

          // set the display encoder flag
          if ((displayEnc==2)||(displayEnc==3)) // already displaying encoder
            displayEnc=3; // we want to display both encoders
          else
            displayEnc=1; // just display one encoder
        }
        else     
          usbMIDI.sendControlChange(1+page*2*encodersBanked, 1, controlChannel);
      }
    }
    else if (encNewB[page*encodersBanked]<encOldB[page*encodersBanked])
    {
      if (debug) {
        Serial.print(" encoder ");
        Serial.print(1);
        Serial.println(" << ");
      }
      else {//for ableton
        if (abletonEncoder) {
          usbMIDI.sendControlChange(1+page*2*encodersBanked, 58, controlChannel);
          encoderLedValueB[page*encodersBanked]-=encLedOffset;
          encoderLedValueB[page*encodersBanked] = constrain(encoderLedValueB[page*encodersBanked], 0, 127);

          // set the display encoder flag
          if ((displayEnc==2)||(displayEnc==3)) // already displaying encoder
            displayEnc=3; // we want to display both encoders
          else
            displayEnc=1; // just display one encoder
        }
        else 
          usbMIDI.sendControlChange(1+page*2*encodersBanked, 127, controlChannel);
      }
    }

    encOldB[page*encodersBanked] = encNewB[page*encodersBanked];
    displayTimer=millis(); // set the time
  }
  interrupts();
}

void readEncoderA() {
  noInterrupts();
  encNewA[page*encodersBanked] = encA.read();
  //encoderLedValueB[page] = constrain(newPositionB[page],0,127);

  if (encNewA[page*encodersBanked] != encOldA[page*encodersBanked]) {

    if (encNewA[page*encodersBanked]>encOldA[page*encodersBanked]) {
      if (debug) {
        Serial.print(" encoder ");
        Serial.print(1);
        Serial.println(" >> ");
      }
      else {
        if (abletonEncoder) {
          usbMIDI.sendControlChange(page*2*encodersBanked, 70, controlChannel);

          encoderLedValueA[page*encodersBanked]+=encLedOffset;
          encoderLedValueA[page*encodersBanked] = constrain(encoderLedValueA[page*encodersBanked], 0, 127);
          // set the display encoder flag
          if ((displayEnc==1)||(displayEnc==3)) // already displaying encoder
            displayEnc=3; // we want to display both encoders
          else
            displayEnc=2; // just display one encoder
        }
        else    
          usbMIDI.sendControlChange(page*2*encodersBanked, 1, controlChannel);
      }
    }
    else if (encNewA[page*encodersBanked]<encOldA[page*encodersBanked])
    {
      if (debug) {
        Serial.print(" encoder ");
        Serial.print(1);
        Serial.println(" << ");
      }
      else {//for ableton
        if (abletonEncoder) {
          usbMIDI.sendControlChange(page*2*encodersBanked, 58, controlChannel);

          encoderLedValueA[page*encodersBanked]-=encLedOffset;
          encoderLedValueA[page*encodersBanked] = constrain(encoderLedValueA[page*encodersBanked], 0, 127);
          // set the display encoder flag
          if ((displayEnc==1)||(displayEnc==3)) // already displaying encoder
            displayEnc=3; // we want to display both encoders
          else
            displayEnc=2; // just display one encoder
        }
        else 
          usbMIDI.sendControlChange(page*2*encodersBanked, 127, controlChannel);
      }
    }

    encOldA[page*encodersBanked] = encNewA[page*encodersBanked];
    displayTimer=millis(); // set the time
  }
  interrupts();
}

// #####################################################################################################################
void changePage() {
  noInterrupts();
  int state;
  int bState;

  //row
  for (int row=0; row<4; row++) {
    _buttonpad.writeGPIO(240-(1<<buttonRow[row]));
    state = _buttonpad.readGPIO();    
    for (int col =0; col<4; col++) {

      bState=state&(1<<col);

      if ((bState==0)&&(bState!=buttons[row][col])&&((millis() - buttonsBounce[row][col]) >bounceDelay)) { //pressed
        buttonsBounce[row][col]=millis();
        int p=(4*row+col)+(col*3-row*3);
        
        if ((stepSequencer)&&(p==15)) // step sequencer
          sequencerPaused=!sequencerPaused;
          
        if((stepSequencer)&&(p==13)&&(hasPattern[selectedChannel])){ // delete sequence
          for(int i=0; i<STEPS; i++)
            stepState[selectedPattern[selectedChannel]][selectedChannel][i]=false;
          muted[selectedChannel]=false;
          }
        
        if((stepSequencer)&&(p==12)){
          savePatterns(selectedChannel);
          savePatternAnimation();
        }
        
        if((stepSequencer)&&((p>7)&&(p<12))){
          selectedPattern[selectedChannel]=p-8;
        }
          
        if((stepSequencer)&&(p==14)&&(hasPattern[selectedChannel])){ // mute channel
          muted[selectedChannel]=!muted[selectedChannel];
        }
        else if (p<8) { // change page
          page = p;
          //Serial.print(" Page ");
          //Serial.println(p);
        }
        //midiNoteOnOff(true,(4*row+col)+(col*3-row*3));
        //setBufferPixel(random(1,7),col,row);
      }
      
      buttons[row][col] = bState;
    }
  }
  interrupts();
  clearBuffer();
  
  if (stepSequencer) {
    
     for (int i=0; i<16; i++){
        hasPattern[i]=false;
        for (int j=0; j<STEPS; j++) {
          if (stepState[selectedPattern[i]][i][j]){
            hasPattern[i]=true;
          }
        }
      }
    
    if(hasPattern[selectedChannel]){
      setBufferPixel(RED, 13);
      if(muted[selectedChannel])
        setBufferPixel(YELLOW, 14);
      else
        setBufferPixel(GREEN, 14);
    }
    setBufferPixel(WHITE, 5);

    if (currentStep<16) {
      setBufferPixel(CYAN, 6);
    }
    else
      setBufferPixel(BLUE, 6);

    if (STEPS==32) {
      if (currentStep>15)
        setBufferPixel(CYAN, 7);
      else
        setBufferPixel(BLUE, 7);
    }
    
  if (!sequencerPaused)
    setBufferPixel(BLUE, 15);
  else
    setBufferPixel(YELLOW, 15);
    
    //show the sellected pattern
    setBufferPixel(PINK, selectedPattern[selectedChannel]+8);
  }

  setBufferPixel(PINK, page);
}

// ####################################################################################################################

void RealTimeSystem(byte realtimebyte) { 

  if (realtimebyte == 248) { 
    clockCounter++; 

    if (clockCounter%6 == 0) { 
      currentStep++;

      if (currentStep==STEPS)
        currentStep=0;

      checkStep();
    }

    if (clockCounter == 24) { 
      clockCounter = 0;
      //digitalWrite(6, HIGH);
    } 

    if (clockCounter == 12) { 
      //digitalWrite(6, LOW);
    }
  } 

  if (realtimebyte == START || realtimebyte == CONTINUE) { 
    clockCounter = 0; 
    setPagePixel(0, 6+currentStep/STEPS, currentStep%16);
    currentStep=0; 
    setPagePixel(BLUE, 6+(currentStep/STEPS), currentStep%16);
    checkStep();
    //digitalWrite(6, HIGH);
  }

  if (realtimebyte == STOP) {
    setPagePixel(0, 6+currentStep/STEPS, currentStep%16);
    currentStep=0;
    setPagePixel(BLUE, 6+currentStep/STEPS, currentStep%16);
    //digitalWrite(6, LOW);
  }
} 

//######################################################################################
void OnCC(byte channel, byte control, byte value) {
  // add all your output component sets that will trigger with cc
  //led.setOn(channel,control,value);
  if (channel==controlChannel) {
    if ((control>=0)&&(control<16)) {// its in range
      int enc=control%2; //find the encoder
      int pg=control/2; // find the bank


      if (enc==0) { // its first encoder
        encoderLedValueA[pg*encodersBanked]=value;
        encoderLedValueA[pg*encodersBanked]=constrain(encoderLedValueA[pg*encodersBanked], 0, 127);

        if (pg*encodersBanked==page*encodersBanked) { // if on the same page display
          // set the display encoder flag
          if ((displayEnc==1)||(displayEnc==3)) // already displaying encoder
            displayEnc=3; // we want to display both encoders
          else
            displayEnc=2; // just display one encoder
          displayTimer=millis(); // set the time
        }
      }
      if (enc==1) { // its second encoder

        encoderLedValueB[pg*encodersBanked]=value;
        encoderLedValueB[pg*encodersBanked]=constrain(encoderLedValueB[pg*encodersBanked], 0, 127);

        if (pg*encodersBanked==page*encodersBanked) { // if on the same page display
          // set the display encoder flag
          if ((displayEnc==2)||(displayEnc==3)) // already displaying encoder
            displayEnc=3; // we want to display both encoders
          else
            displayEnc=1; // just display one encoder
          displayTimer=millis(); // set the time
        }
      }
    }
  }
}

//######################################################################################

void checkStep() {
  if ((stepSequencer)&&(!sequencerPaused)) {
    for (int i=0; i<16; i++) {  
    if (lastPlayed[i]) {
        //midiNoteOnOff(false, i-1+7*16);
        usbMIDI.sendNoteOff(i+36, 0, sequencerChannel);
        lastPlayed[i]=false;
        //lastPlayed[i-1]=false;
        //setLedOff(i-1,6);
      }

      if ((stepState[selectedPattern[i]][i][currentStep])&&(!muted[i])) {
        usbMIDI.sendNoteOn(i+36, 127, sequencerChannel);
        lastPlayed[i]=true;
        //setPagePixel(0, 5, i);
        //setLedOn(i,6);
      }
      else {
        lastPlayed[i]=false;
      }
    }
    //setPagePixel(WHITE, 5, selectedChannel);
  }
}

//######################################################################################
//load eeprom
void loadSetup(){
  int address = 0;
  byte evalue;
  
  evalue = EEPROM.read(address);
  
  //Serial.println(evalue);
  
  if(evalue==2){ // load rest of settings
  
    midiChannel=EEPROM.read(1)%16; // midi channel number
    controlChannel=EEPROM.read(2)%16; // midi channel number
    sequencerChannel=EEPROM.read(3)%16; // midi channel number
    if(EEPROM.read(4)==1) // enable secondary midi messages
      secondary=true;
    else
      secondary=false;
    if(EEPROM.read(5)==1) // enable ableton encoder mode
      abletonEncoder=true;
    else
      abletonEncoder=false;
    if(EEPROM.read(6)==1)
      stepSequencer=true;
    else
      stepSequencer=false;
      
    STEPS=EEPROM.read(7); //lenth of steps
    
   for(int i=0; i<8; i++){ // default colors
        pageColor[i]=EEPROM.read(8+i)%8;
    }
    encodersBanked=EEPROM.read(16); //bank encoders
    
    loadPatterns();// load sequencer patterns
    
    
  }else{ // set default settings
    pageColor[0]=GREEN;
    pageColor[1]=YELLOW;
    pageColor[2]=BLUE;
    pageColor[3]=RED;
    pageColor[4]=pageColor[5]=pageColor[6]=pageColor[7]=GREEN;

    midiChannel=1; // midi channel number
    controlChannel=2; // midi channel number
    sequencerChannel=3; // midi channel number
    secondary=false; // enable secondary midi messages
    abletonEncoder=false;
    stepSequencer=true;
    STEPS=32; //lenth of steps
    encodersBanked=1;
    
    saveSetup();
    
    for(int i=0;i<16;i++){ //save blank patterns to avoid errors on loading
      savePatterns(i);
    }
  }
}

//######################################################################################
//save eeprom
void saveSetup(){
  int address = 0;
  
  byte evalue = 2;
  EEPROM.write(address, evalue); // save a settings flag
  
  EEPROM.write(1, (byte)midiChannel); // save middi channels
  EEPROM.write(2, (byte)controlChannel);
  EEPROM.write(3, (byte)sequencerChannel);
  
  if(secondary) // save secondary buttons
    EEPROM.write(4, 1);
  else
    EEPROM.write(4, 0);
    
  if(abletonEncoder) // save ableton encoder mode
    EEPROM.write(5, 1);
  else
    EEPROM.write(5, 0);
    
  if(stepSequencer) // save sequencer default
    EEPROM.write(6, 1);
  else
    EEPROM.write(6, 0);
    
  EEPROM.write(7, (byte)STEPS); // save sequencer length
  
  for(int i=0; i<8; i++){ // save colors
    EEPROM.write(8+i,(byte)pageColor[i]);
  }
  
  if(encodersBanked==1) // save encoders banked
    EEPROM.write(16, 1);
  else
    EEPROM.write(16, 0);
}

//######################################################################################
// display settings menu
void setupMenu(){
  
  while(setupMode){
    readSettingsButtonsGPIO();
    
    switch(setupPage){
      case 0: // main setup menu
        setBufferPixel(GREEN, 15); // exit button
        setBufferPixel(YELLOW, 4); // button channel
        setBufferPixel(YELLOW, 5); // control channel
        setBufferPixel(YELLOW, 6); // sequencer channel
        if(secondary)
          setBufferPixel(PINK, 7); // secondary button messages
        else
          setBufferPixel(WHITE, 7);
          
        if(abletonEncoder)
          setBufferPixel(PINK, 0); // ableton encoder mode
        else
          setBufferPixel(WHITE, 0); // ableton encoder mode
          
        if(encodersBanked==1)
          setBufferPixel(PINK, 1); // ableton encoder mode
        else
          setBufferPixel(WHITE, 1); // ableton encoder mode
          
        if(stepSequencer)
          setBufferPixel(PINK, 2); // ableton encoder mode
        else
          setBufferPixel(WHITE, 2); // ableton encoder mode
          
        if(STEPS==32)
          setBufferPixel(PINK, 3); // ableton encoder mode
        else
          setBufferPixel(WHITE, 3); // ableton encoder mode
        setBufferPixel(CYAN, 11); // color menu
        break;
      case 1: // button channel
        setBufferPixel(YELLOW, midiChannel-1);
        break;
      case 2: // control channel
        setBufferPixel(YELLOW, controlChannel-1);
        break;
      case 3: // control channel
        setBufferPixel(YELLOW, sequencerChannel-1);
        break;
      case 4: // page colors
        setBufferPixel(GREEN, 15); // exit button
        for(int i=0; i<8; i++){
          setBufferPixel(pageColor[i], i);
        }
        break;
    }
    
    printBuffer();
    
  }
  saveSetup();
  clearBuffer();
}

//######################################################################################
void readSettingsButtonsGPIO() {
  noInterrupts();
  int state;
  int bState;

  //row
  for (int row=0; row<4; row++) {
    _buttonpad.writeGPIO(240-(1<<buttonRow[row]));
    state = _buttonpad.readGPIO();    
    for (int col =0; col<4; col++) {

      bState=state&(1<<col);

      if ((bState==0)&&(bState!=buttons[row][col])&&((millis() - buttonsBounce[row][col]) >bounceDelay)) {
        buttonsBounce[row][col]=millis();
        int p=(4*row+col)+(col*3-row*3);
        
        //Serial.println(p);
        
        if(setupPage==1){ // button channel
          midiChannel=p+1;
          setupPage=0;
          clearBuffer();
          return;
        }
        if(setupPage==2){ // control channel
          controlChannel=p+1;
          setupPage=0;
          clearBuffer();
          return;
        }
        if(setupPage==3){ // control channel
          sequencerChannel=p+1;
          setupPage=0;
          clearBuffer();
          return;
        }
        if(setupPage==4){ // page color menu
          if(p==15){// close color menu
            setupPage=0;
            clearBuffer();
            return;
          }
          if(p<8){
            pageColor[p]=(pageColor[p]+1)%7+1;
          }
        }

        if(setupPage==0){
          if(p==15){ // escape button
            setupMode=false;
          }
          if(p==4){ // button channel
            setupPage=1;
            //Serial.println("button channel");
            clearBuffer();
          }
          if(p==5){ // control channel
            setupPage=2;
            //Serial.println("control channel");
            clearBuffer();
          }
          if(p==6){ // sequencer channel
            setupPage=3;
            //Serial.println("sequencer channel");
            clearBuffer();
          }
          if(p==7){ // secondary messages
            secondary=!secondary;
            clearBuffer();
          }
          if(p==0){ // ableton encoder mode
            abletonEncoder=!abletonEncoder;
            clearBuffer();
          }
          
          if(p==1){ // encoders banked
            if(encodersBanked==1)
              encodersBanked=0;
            else
              encodersBanked=1;
              
            clearBuffer();
          }
          
          if(p==2){ // sequencer enabled
            stepSequencer=!stepSequencer;
            clearBuffer();
          }
          if(p==3){ // sequencer length
            if(STEPS==16)
              STEPS=32;
            else
              STEPS=16;
            clearBuffer();
          }
          if(p==11){ // page colors
            setupPage=4;
            clearBuffer();
          }
        }
        
      }
      buttons[row][col] = bState;
    }
  }
  interrupts();
}

//######################################################################################
void loadPatterns(){
  int pattern[4]={0,0,0,0};
  
  for(int v=0;v<16;v++){

    for(int p=0;p<4;p++){
      
      pattern[0]=EEPROM.read(17+v*16+p*4+0);
      pattern[1]=EEPROM.read(17+v*16+p*4+1);
      pattern[2]=EEPROM.read(17+v*16+p*4+2);
      pattern[3]=EEPROM.read(17+v*16+p*4+3);
      
      for (int i=0;i<8;i++){
        stepState[p][v][i]=(((pattern[0]>>i)&1)==1);
        stepState[p][v][i+8]=(((pattern[1]>>i)&1)==1);
        stepState[p][v][i+16]=(((pattern[2]>>i)&1)==1);
        stepState[p][v][i+24]=(((pattern[3]>>i)&1)==1);
      }
    }
  }
}

//######################################################################################
void savePatterns(int v){
  int pattern[4]={0,0,0,0}; // we can write to the epprom only a byte. ei 8 steps but we have 32
  
  for(int p=0;p<4;p++){
    pattern[0]=pattern[1]=pattern[2]=pattern[3]=0;
    
    for(int i=0;i<8;i++){
      if(stepState[p][v][i])
        pattern[0]+=1<<i;
      if(stepState[p][v][i+8])
        pattern[1]+=1<<i;
      if(stepState[p][v][i+16])
        pattern[2]+=1<<i;
      if(stepState[p][v][i+24])
        pattern[3]+=1<<i;
    }
    // write pattern to eeprom
    EEPROM.write(17+v*16+p*4+0, pattern[0]);
    EEPROM.write(17+v*16+p*4+1, pattern[1]);
    EEPROM.write(17+v*16+p*4+2, pattern[2]);
    EEPROM.write(17+v*16+p*4+3, pattern[3]);
    }
  
}

//######################################################################################
//save pattern animation
void savePatternAnimation(){
  clearBuffer();
    for ( int i=0; i<16; i++) {
      setBufferPixel(RED, i);
      printBuffer();
    }
  printBuffer();
  delay(5);
  clearBuffer();
}
