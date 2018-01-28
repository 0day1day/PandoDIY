
#if defined(ARDUINO) && ARDUINO >= 100
  #include "Arduino.h"
#else
  #include "WProgram.h"
  #include <pins_arduino.h>
#endif


#include "Pando.h"
#include <Oscillator.h>
// #include <US.h>



void Pando::init(int YL, int YR, int RL, int RR, bool load_calibration, int NoiseSensor, int Buzzer/*, int USTrigger, int USEcho*/) {
  
  servo_pins[0] = YL;
  servo_pins[1] = YR;
  servo_pins[2] = RL;
  servo_pins[3] = RR;

  attachServos();
  isPandoResting=false;

  if (load_calibration) {
    for (int i = 0; i < 4; i++) {
      int servo_trim = EEPROM.read(i);
      if (servo_trim > 128) servo_trim -= 256;
      servo[i].SetTrim(servo_trim);
    }
  }
  
  for (int i = 0; i < 4; i++) servo_position[i] = 90;

  // US sensor init with the pins:
  // us.init(USTrigger, USEcho);

  // Buzzer & noise sensor pins: 
  pinBuzzer = Buzzer;
  pinNoiseSensor = NoiseSensor;

  pinMode(Buzzer,OUTPUT);
  pinMode(NoiseSensor,INPUT);

  // FireBettle LED Matrix setup
  ht1632c.begin();
  ht1632c.isLedOn(true);
  ht1632c.clearScreen();
}

///////////////////////////////////////////////////////////////////
//-- ATTACH & DETACH FUNCTIONS ----------------------------------//
///////////////////////////////////////////////////////////////////
void Pando::attachServos(){
    servo[0].attach(servo_pins[0]);
    servo[1].attach(servo_pins[1]);
    servo[2].attach(servo_pins[2]);
    servo[3].attach(servo_pins[3]);
}

void Pando::detachServos(){
    servo[0].detach();
    servo[1].detach();
    servo[2].detach();
    servo[3].detach();
}

///////////////////////////////////////////////////////////////////
//-- OSCILLATORS TRIMS ------------------------------------------//
///////////////////////////////////////////////////////////////////
void Pando::setTrims(int YL, int YR, int RL, int RR) {
  servo[0].SetTrim(YL);
  servo[1].SetTrim(YR);
  servo[2].SetTrim(RL);
  servo[3].SetTrim(RR);
}

void Pando::saveTrimsOnEEPROM() {
  
  for (int i = 0; i < 4; i++){ 
      EEPROM.write(i, servo[i].getTrim());
  } 
      
}


///////////////////////////////////////////////////////////////////
//-- BASIC MOTION FUNCTIONS -------------------------------------//
///////////////////////////////////////////////////////////////////
void Pando::_moveServos(int time, int  servo_target[]) {

  attachServos();
  if(getRestState()==true){
        setRestState(false);
  }

  if(time>10){
    for (int i = 0; i < 4; i++) increment[i] = ((servo_target[i]) - servo_position[i]) / (time / 10.0);
    final_time =  millis() + time;

    for (int iteration = 1; millis() < final_time; iteration++) {
      partial_time = millis() + 10;
      for (int i = 0; i < 4; i++) servo[i].SetPosition(servo_position[i] + (iteration * increment[i]));
      while (millis() < partial_time); //pause
    }
  }
  else{
    for (int i = 0; i < 4; i++) servo[i].SetPosition(servo_target[i]);
  }
  for (int i = 0; i < 4; i++) servo_position[i] = servo_target[i];
}


void Pando::oscillateServos(int A[4], int O[4], int T, double phase_diff[4], float cycle=1){

  for (int i=0; i<4; i++) {
    servo[i].SetO(O[i]);
    servo[i].SetA(A[i]);
    servo[i].SetT(T);
    servo[i].SetPh(phase_diff[i]);
  }
  double ref=millis();
   for (double x=ref; x<=T*cycle+ref; x=millis()){
     for (int i=0; i<4; i++){
        servo[i].refresh();
     }
  }
}


void Pando::_execute(int A[4], int O[4], int T, double phase_diff[4], float steps = 1.0){

  attachServos();
  if(getRestState()==true){
        setRestState(false);
  }


  int cycles=(int)steps;    

  //-- Execute complete cycles
  if (cycles >= 1) 
    for(int i = 0; i < cycles; i++) 
      oscillateServos(A,O, T, phase_diff);
      
  //-- Execute the final not complete cycle    
  oscillateServos(A,O, T, phase_diff,(float)steps-cycles);
}



///////////////////////////////////////////////////////////////////
//-- HOME = Pando at rest position -------------------------------//
///////////////////////////////////////////////////////////////////
void Pando::home(){

  if(isPandoResting==false){ //Go to rest position only if necessary

    int homes[4]={90, 90, 90, 90}; //All the servos at rest position
    _moveServos(500,homes);   //Move the servos in half a second

    detachServos();
    isPandoResting=true;
  }
}

bool Pando::getRestState(){

    return isPandoResting;
}

void Pando::setRestState(bool state){

    isPandoResting = state;
}


///////////////////////////////////////////////////////////////////
//-- PREDETERMINED MOTION SEQUENCES -----------------------------//
///////////////////////////////////////////////////////////////////

//---------------------------------------------------------
//-- Pando movement: Jump
//--  Parameters:
//--    steps: Number of steps
//--    T: Period
//---------------------------------------------------------
void Pando::jump(float steps, int T){

  int up[]={90,90,150,30};
  _moveServos(T,up);
  int down[]={90,90,90,90};
  _moveServos(T,down);
}


//---------------------------------------------------------
//-- Pando gait: Walking  (forward or backward)    
//--  Parameters:
//--    * steps:  Number of steps
//--    * T : Period
//--    * Dir: Direction: FORWARD / BACKWARD
//---------------------------------------------------------
void Pando::walk(float steps, int T, int dir){

  //-- Oscillator parameters for walking
  //-- Hip sevos are in phase
  //-- Feet servos are in phase
  //-- Hip and feet are 90 degrees out of phase
  //--      -90 : Walk forward
  //--       90 : Walk backward
  //-- Feet servos also have the same offset (for tiptoe a little bit)
  int A[4]= {30, 30, 20, 20};
  int O[4] = {0, 0, 4, -4};
  double phase_diff[4] = {0, 0, DEG2RAD(dir * -90), DEG2RAD(dir * -90)};

  //-- Let's oscillate the servos!
  _execute(A, O, T, phase_diff, steps);  
}


//---------------------------------------------------------
//-- Pando gait: Turning (left or right)
//--  Parameters:
//--   * Steps: Number of steps
//--   * T: Period
//--   * Dir: Direction: LEFT / RIGHT
//---------------------------------------------------------
void Pando::turn(float steps, int T, int dir){

  //-- Same coordination than for walking (see Pando::walk)
  //-- The Amplitudes of the hip's oscillators are not igual
  //-- When the right hip servo amplitude is higher, the steps taken by
  //--   the right leg are bigger than the left. So, the robot describes an 
  //--   left arc
  int A[4]= {30, 30, 20, 20};
  int O[4] = {0, 0, 4, -4};
  double phase_diff[4] = {0, 0, DEG2RAD(-90), DEG2RAD(-90)}; 
    
  if (dir == LEFT) {  
    A[0] = 30; //-- Left hip servo
    A[1] = 10; //-- Right hip servo
  }
  else {
    A[0] = 10;
    A[1] = 30;
  }
    
  //-- Let's oscillate the servos!
  _execute(A, O, T, phase_diff, steps); 
}


//---------------------------------------------------------
//-- Pando gait: Lateral bend
//--  Parameters:
//--    steps: Number of bends
//--    T: Period of one bend
//--    dir: RIGHT=Right bend LEFT=Left bend
//---------------------------------------------------------
void Pando::bend(int steps, int T, int dir){

  //Parameters of all the movements. Default: Left bend
  int bend1[4]={90, 90, 62, 35}; 
  int bend2[4]={90, 90, 62, 105};
  int homes[4]={90, 90, 90, 90};

  //Time of one bend, constrained in order to avoid movements too fast.
  //T=max(T, 600);

  //Changes in the parameters if right direction is chosen 
  if(dir==-1)
  {
    bend1[2]=180-35;
    bend1[3]=180-60;  //Not 65. Pando is unbalanced
    bend2[2]=180-105;
    bend2[3]=180-60;
  }

  //Time of the bend movement. Fixed parameter to avoid falls
  int T2=800; 

  //Bend movement
  for (int i=0;i<steps;i++)
  {
    _moveServos(T2/2,bend1);
    _moveServos(T2/2,bend2);
    delay(T*0.8);
    _moveServos(500,homes);
  }

}


//---------------------------------------------------------
//-- Pando gait: Shake a leg
//--  Parameters:
//--    steps: Number of shakes
//--    T: Period of one shake
//--    dir: RIGHT=Right leg LEFT=Left leg
//---------------------------------------------------------
void Pando::shakeLeg(int steps,int T,int dir){

  //This variable change the amount of shakes
  int numberLegMoves=2;

  //Parameters of all the movements. Default: Right leg
  int shake_leg1[4]={90, 90, 58, 35};   
  int shake_leg2[4]={90, 90, 58, 120};
  int shake_leg3[4]={90, 90, 58, 60};
  int homes[4]={90, 90, 90, 90};

  //Changes in the parameters if left leg is chosen
  if(dir==-1)      
  {
    shake_leg1[2]=180-35;
    shake_leg1[3]=180-58;
    shake_leg2[2]=180-120;
    shake_leg2[3]=180-58;
    shake_leg3[2]=180-60;
    shake_leg3[3]=180-58;
  }
  
  //Time of the bend movement. Fixed parameter to avoid falls
  int T2=1000;    
  //Time of one shake, constrained in order to avoid movements too fast.            
  T=T-T2;
  T=max(T,200*numberLegMoves);  

  for (int j=0; j<steps;j++)
  {
  //Bend movement
  _moveServos(T2/2,shake_leg1);
  _moveServos(T2/2,shake_leg2);
  
    //Shake movement
    for (int i=0;i<numberLegMoves;i++)
    {
    _moveServos(T/(2*numberLegMoves),shake_leg3);
    _moveServos(T/(2*numberLegMoves),shake_leg2);
    }
    _moveServos(500,homes); //Return to home position
  }
  
  delay(T);
}


//---------------------------------------------------------
//-- Pando movement: up & down
//--  Parameters:
//--    * steps: Number of jumps
//--    * T: Period
//--    * h: Jump height: SMALL / MEDIUM / BIG 
//--              (or a number in degrees 0 - 90)
//---------------------------------------------------------
void Pando::updown(float steps, int T, int h){

  //-- Both feet are 180 degrees out of phase
  //-- Feet amplitude and offset are the same
  //-- Initial phase for the right foot is -90, so that it starts
  //--   in one extreme position (not in the middle)
  int A[4]= {0, 0, h, h};
  int O[4] = {0, 0, h, -h};
  double phase_diff[4] = {0, 0, DEG2RAD(-90), DEG2RAD(90)};
  
  //-- Let's oscillate the servos!
  _execute(A, O, T, phase_diff, steps); 
}


//---------------------------------------------------------
//-- Pando movement: swinging side to side
//--  Parameters:
//--     steps: Number of steps
//--     T : Period
//--     h : Amount of swing (from 0 to 50 aprox)
//---------------------------------------------------------
void Pando::swing(float steps, int T, int h){

  //-- Both feets are in phase. The offset is half the amplitude
  //-- It causes the robot to swing from side to side
  int A[4]= {0, 0, h, h};
  int O[4] = {0, 0, h/2, -h/2};
  double phase_diff[4] = {0, 0, DEG2RAD(0), DEG2RAD(0)};
  
  //-- Let's oscillate the servos!
  _execute(A, O, T, phase_diff, steps); 
}


//---------------------------------------------------------
//-- Pando movement: swinging side to side without touching the floor with the heel
//--  Parameters:
//--     steps: Number of steps
//--     T : Period
//--     h : Amount of swing (from 0 to 50 aprox)
//---------------------------------------------------------
void Pando::tiptoeSwing(float steps, int T, int h){

  //-- Both feets are in phase. The offset is not half the amplitude in order to tiptoe
  //-- It causes the robot to swing from side to side
  int A[4]= {0, 0, h, h};
  int O[4] = {0, 0, h, -h};
  double phase_diff[4] = {0, 0, 0, 0};
  
  //-- Let's oscillate the servos!
  _execute(A, O, T, phase_diff, steps); 
}


//---------------------------------------------------------
//-- Pando gait: Jitter 
//--  Parameters:
//--    steps: Number of jitters
//--    T: Period of one jitter 
//--    h: height (Values between 5 - 25)   
//---------------------------------------------------------
void Pando::jitter(float steps, int T, int h){

  //-- Both feet are 180 degrees out of phase
  //-- Feet amplitude and offset are the same
  //-- Initial phase for the right foot is -90, so that it starts
  //--   in one extreme position (not in the middle)
  //-- h is constrained to avoid hit the feets
  h=min(25,h);
  int A[4]= {h, h, 0, 0};
  int O[4] = {0, 0, 0, 0};
  double phase_diff[4] = {DEG2RAD(-90), DEG2RAD(90), 0, 0};
  
  //-- Let's oscillate the servos!
  _execute(A, O, T, phase_diff, steps); 
}


//---------------------------------------------------------
//-- Pando gait: Ascending & turn (Jitter while up&down)
//--  Parameters:
//--    steps: Number of bends
//--    T: Period of one bend
//--    h: height (Values between 5 - 15) 
//---------------------------------------------------------
void Pando::ascendingTurn(float steps, int T, int h){

  //-- Both feet and legs are 180 degrees out of phase
  //-- Initial phase for the right foot is -90, so that it starts
  //--   in one extreme position (not in the middle)
  //-- h is constrained to avoid hit the feets
  h=min(13,h);
  int A[4]= {h, h, h, h};
  int O[4] = {0, 0, h+4, -h+4};
  double phase_diff[4] = {DEG2RAD(-90), DEG2RAD(90), DEG2RAD(-90), DEG2RAD(90)};
  
  //-- Let's oscillate the servos!
  _execute(A, O, T, phase_diff, steps); 
}


//---------------------------------------------------------
//-- Pando gait: Moonwalker. Pando moves like Michael Jackson
//--  Parameters:
//--    Steps: Number of steps
//--    T: Period
//--    h: Height. Typical valures between 15 and 40
//--    dir: Direction: LEFT / RIGHT
//---------------------------------------------------------
void Pando::moonwalker(float steps, int T, int h, int dir){

  //-- This motion is similar to that of the caterpillar robots: A travelling
  //-- wave moving from one side to another
  //-- The two Pando's feet are equivalent to a minimal configuration. It is known
  //-- that 2 servos can move like a worm if they are 120 degrees out of phase
  //-- In the example of Pando, the two feet are mirrored so that we have:
  //--    180 - 120 = 60 degrees. The actual phase difference given to the oscillators
  //--  is 60 degrees.
  //--  Both amplitudes are equal. The offset is half the amplitud plus a little bit of
  //-   offset so that the robot tiptoe lightly
 
  int A[4]= {0, 0, h, h};
  int O[4] = {0, 0, h/2+2, -h/2 -2};
  int phi = -dir * 90;
  double phase_diff[4] = {0, 0, DEG2RAD(phi), DEG2RAD(-60 * dir + phi)};
  
  //-- Let's oscillate the servos!
  _execute(A, O, T, phase_diff, steps); 
}


//----------------------------------------------------------
//-- Pando gait: Crusaito. A mixture between moonwalker and walk
//--   Parameters:
//--     steps: Number of steps
//--     T: Period
//--     h: height (Values between 20 - 50)
//--     dir:  Direction: LEFT / RIGHT
//-----------------------------------------------------------
void Pando::crusaito(float steps, int T, int h, int dir){

  int A[4]= {25, 25, h, h};
  int O[4] = {0, 0, h/2+ 4, -h/2 - 4};
  double phase_diff[4] = {90, 90, DEG2RAD(0), DEG2RAD(-60 * dir)};
  
  //-- Let's oscillate the servos!
  _execute(A, O, T, phase_diff, steps); 
}


//---------------------------------------------------------
//-- Pando gait: Flapping
//--  Parameters:
//--    steps: Number of steps
//--    T: Period
//--    h: height (Values between 10 - 30)
//--    dir: direction: FOREWARD, BACKWARD
//---------------------------------------------------------
void Pando::flapping(float steps, int T, int h, int dir){

  int A[4]= {12, 12, h, h};
  int O[4] = {0, 0, h - 10, -h + 10};
  double phase_diff[4] = {DEG2RAD(0), DEG2RAD(180), DEG2RAD(-90 * dir), DEG2RAD(90 * dir)};
  
  //-- Let's oscillate the servos!
  _execute(A, O, T, phase_diff, steps); 
}






///////////////////////////////////////////////////////////////////
//-- SENSORS FUNCTIONS  -----------------------------------------//
///////////////////////////////////////////////////////////////////

//---------------------------------------------------------
//-- Pando getDistance: return Pando's ultrasonic sensor measure
//---------------------------------------------------------
// float Pando::getDistance(){

//   return us.read();
// }


//---------------------------------------------------------
//-- Pando getNoise: return Pando's noise sensor measure
//---------------------------------------------------------
int Pando::getNoise(){

  int noiseLevel = 0;
  int noiseReadings = 0;
  int numReadings = 2;  

    noiseLevel = analogRead(pinNoiseSensor);

    for(int i=0; i<numReadings; i++){
        noiseReadings += analogRead(pinNoiseSensor);
        delay(4); // delay in between reads for stability
    }

    noiseLevel = noiseReadings / numReadings;

    return noiseLevel;
}


//---------------------------------------------------------
//-- Pando getBatteryLevel: return battery voltage percent
//---------------------------------------------------------
double Pando::getBatteryLevel(){

  //The first read of the batery is often a wrong reading, so we will discard this value. 
    double batteryLevel = battery.readBatPercent();
    double batteryReadings = 0;
    int numReadings = 10;

    for(int i=0; i<numReadings; i++){
        batteryReadings += battery.readBatPercent();
        delay(1); // delay in between reads for stability
    }

    batteryLevel = batteryReadings / numReadings;

    return batteryLevel;
}


double Pando::getBatteryVoltage(){

  //The first read of the batery is often a wrong reading, so we will discard this value. 
    double batteryLevel = battery.readBatVoltage();
    double batteryReadings = 0;
    int numReadings = 10;

    for(int i=0; i<numReadings; i++){
        batteryReadings += battery.readBatVoltage();
        delay(1); // delay in between reads for stability
    }

    batteryLevel = batteryReadings / numReadings;

    return batteryLevel;
}


///////////////////////////////////////////////////////////////////
//-- MOUTHS & ANIMATIONS ----------------------------------------//
///////////////////////////////////////////////////////////////////

// unsigned long int Pando::getMouthShape(int number){
//   unsigned long int types []={zero_code,one_code,two_code,three_code,four_code,five_code,six_code,seven_code,eight_code,
//   nine_code,smile_code,happyOpen_code,happyClosed_code,heart_code,bigSurprise_code,smallSurprise_code,tongueOut_code,
//   vamp1_code,vamp2_code,lineMouth_code,confused_code,diagonal_code,sad_code,sadOpen_code,sadClosed_code,
//   okMouth_code, xMouth_code,interrogation_code,thunder_code,culito_code,angry_code};

//   return types[number];
// }


// unsigned long int Pando::getAnimShape(int anim, int index){

//   unsigned long int littleUuh_code[]={
//      0b00000000000000001100001100000000,
//      0b00000000000000000110000110000000,
//      0b00000000000000000011000011000000,
//      0b00000000000000000110000110000000,
//      0b00000000000000001100001100000000,
//      0b00000000000000011000011000000000,
//      0b00000000000000110000110000000000,
//      0b00000000000000011000011000000000  
//   };

//   unsigned long int dreamMouth_code[]={
//      0b00000000000000000000110000110000,
//      0b00000000000000010000101000010000,  
//      0b00000000011000100100100100011000,
//      0b00000000000000010000101000010000           
//   };

//   unsigned long int adivinawi_code[]={
//      0b00100001000000000000000000100001,
//      0b00010010100001000000100001010010,
//      0b00001100010010100001010010001100,
//      0b00000000001100010010001100000000,
//      0b00000000000000001100000000000000,
//      0b00000000000000000000000000000000
//   };

//   unsigned long int wave_code[]={
//      0b00001100010010100001000000000000,
//      0b00000110001001010000100000000000,
//      0b00000011000100001000010000100000,
//      0b00000001000010000100001000110000,
//      0b00000000000001000010100100011000,
//      0b00000000000000100001010010001100,
//      0b00000000100000010000001001000110,
//      0b00100000010000001000000100000011,
//      0b00110000001000000100000010000001,
//      0b00011000100100000010000001000000    
//   };

//   switch  (anim){

//     case littleUuh:
//         return littleUuh_code[index];
//         break;
//     case dreamMouth:
//         return dreamMouth_code[index];
//         break;
//     case adivinawi:
//         return adivinawi_code[index];
//         break;
//     case wave:
//         return wave_code[index];
//         break;    
//   }   
// }


// void Pando::putAnimationMouth(unsigned long int aniMouth, int index){

//       ledmatrix.writeFull(getAnimShape(aniMouth,index));
// }


// void Pando::putMouth(unsigned long int mouth, bool predefined){

//   if (predefined){
//     ledmatrix.writeFull(getMouthShape(mouth));
//   }
//   else{
//     ledmatrix.writeFull(mouth);
//   }
// }


// void Pando::clearMouth(){

//   ledmatrix.clearMatrix();
// }







///////////////////////////////////////////////////////////////////
//-- EYES & ANIMATIONS ----------------------------------------//
///////////////////////////////////////////////////////////////////

void Pando::putEyes(int eyeExpression) {
  switch (eyeExpression) {
      case smile:
        smileEyes();
        break;
      case happyOpen:
        happyOpenEyes();
        break;
      case happyClosed:
        closeEyes();
        break;
      case heart:
        loveEyes();
        break;
      case bigSurprise:
        // do something
        break;
      case smallSurprise:
        // do something
        break;
      case confused:
        confusedEyes();
        break;
      case sad:
        sadEyes();
        break;
      case sadOpen:
        sadOpenEyes();
        break;
      case sadClosed:
        sadCloseEyes();
        break;   
      case angry:
        // do something
        angryEyes();
        break;
      case fartLeft:
        fartLeftEyes();
        break;
      case fartRight:
        fartRightEyes();
        break;
  }
}


// void Pando::putAnimationEye(unsigned long int aniMouth, int index) {

// }








///////////////////////////////////////////////////////////////////
//-- SOUNDS -----------------------------------------------------//
///////////////////////////////////////////////////////////////////

void Pando::_tone (float noteFrequency, long noteDuration, int silentDuration){

    // tone(10,261,500);
    // delay(500);

      if(silentDuration==0){silentDuration=1;}

      tone(Pando::pinBuzzer, noteFrequency, noteDuration);
      delay(noteDuration);       //milliseconds to microseconds
      //noTone(PIN_Buzzer);
      delay(silentDuration);     
}


void Pando::bendTones (float initFrequency, float finalFrequency, float prop, long noteDuration, int silentDuration){

  //Examples:
  //  bendTones (880, 2093, 1.02, 18, 1);
  //  bendTones (note_A5, note_C7, 1.02, 18, 0);

  if(silentDuration==0){silentDuration=1;}

  if(initFrequency < finalFrequency)
  {
      for (int i=initFrequency; i<finalFrequency; i=i*prop) {
          _tone(i, noteDuration, silentDuration);
      }

  } else{

      for (int i=initFrequency; i>finalFrequency; i=i/prop) {
          _tone(i, noteDuration, silentDuration);
      }
  }
}


void Pando::sing(int songName){
  switch(songName){

    case S_connection:
      _tone(note_E5, 50, 30);
      _tone(note_E6, 55, 25);
      _tone(note_A6, 60, 10);
    break;

    case S_disconnection:
      _tone(note_E5, 50, 30);
      _tone(note_A6, 55, 25);
      _tone(note_E6, 50, 10);
    break;

    case S_buttonPushed:
      bendTones (note_E6, note_G6, 1.03, 20, 2);
      delay(30);
      bendTones (note_E6, note_D7, 1.04, 10, 2);
    break;

    case S_mode1:
      bendTones (note_E6, note_A6, 1.02, 30, 10);  //1318.51 to 1760
    break;

    case S_mode2:
      bendTones (note_G6, note_D7, 1.03, 30, 10);  //1567.98 to 2349.32
    break;

    case S_mode3:
      _tone(note_E6, 50, 100); //D6
      _tone(note_G6, 50, 80);  //E6
      _tone(note_D7, 300, 0);  //G6
    break;

    case S_surprise:
      bendTones(800, 2150, 1.02, 10, 1);
      bendTones(2149, 800, 1.03, 7, 1);
    break;

    case S_OhOoh:
      bendTones(880, 2000, 1.04, 8, 3); //A5 = 880
      delay(200);

      for (int i=880; i<2000; i=i*1.04) {
           _tone(note_B5,5,10);
      }
    break;

    case S_OhOoh2:
      bendTones(1880, 3000, 1.03, 8, 3);
      delay(200);

      for (int i=1880; i<3000; i=i*1.03) {
          _tone(note_C6,10,10);
      }
    break;

    case S_cuddly:
      bendTones(700, 900, 1.03, 16, 4);
      bendTones(899, 650, 1.01, 18, 7);
    break;

    case S_sleeping:
      bendTones(100, 500, 1.04, 10, 10);
      delay(500);
      bendTones(400, 100, 1.04, 10, 1);
    break;

    case S_happy:
      bendTones(1500, 2500, 1.05, 20, 8);
      bendTones(2499, 1500, 1.05, 25, 8);
    break;

    case S_superHappy:
      bendTones(2000, 6000, 1.05, 8, 3);
      delay(50);
      bendTones(5999, 2000, 1.05, 13, 2);
    break;

    case S_happy_short:
      bendTones(1500, 2000, 1.05, 15, 8);
      delay(100);
      bendTones(1900, 2500, 1.05, 10, 8);
    break;

    case S_sad:
      bendTones(880, 669, 1.02, 20, 200);
    break;

    case S_confused:
      bendTones(1000, 1700, 1.03, 8, 2); 
      bendTones(1699, 500, 1.04, 8, 3);
      bendTones(1000, 1700, 1.05, 9, 10);
    break;

    case S_fart1:
      bendTones(1600, 3000, 1.02, 2, 15);
    break;

    case S_fart2:
      bendTones(2000, 6000, 1.02, 2, 20);
    break;

    case S_fart3:
      bendTones(1600, 4000, 1.02, 2, 20);
      bendTones(4000, 3000, 1.02, 2, 20);
    break;

  }
}



///////////////////////////////////////////////////////////////////
//-- GESTURES ---------------------------------------------------//
///////////////////////////////////////////////////////////////////

void Pando::playGesture(int gesture){

  int sadPos[4]=      {110, 70, 20, 160};
  int bedPos[4]=      {100, 80, 60, 120};
  int fartPos_1[4]=   {90, 90, 145, 122}; //rightBend
  int fartPos_2[4]=   {90, 90, 80, 122};
  int fartPos_3[4]=   {90, 90, 145, 80};
  int confusedPos[4]= {110, 70, 90, 90};
  int angryPos[4]=    {90, 90, 70, 110};
  int headLeft[4]=    {110, 110, 90, 90};
  int headRight[4]=   {70, 70, 90, 90};
  int fretfulPos[4]=  {90, 90, 90, 110};
  int bendPos_1[4]=   {90, 90, 70, 35};
  int bendPos_2[4]=   {90, 90, 55, 35};
  int bendPos_3[4]=   {90, 90, 42, 35};
  int bendPos_4[4]=   {90, 90, 34, 35};
  
  switch(gesture){

    case PandoHappy: 
        _tone(note_E5,50,30);
        // putMouth(smile);
        putEyes(smile);
        sing(S_happy_short);
        swing(1,800,20); 
        sing(S_happy_short);

        home();
        // putMouth(happyOpen);
        putEyes(happyOpen);
    break;


    case PandoSuperHappy:
        // putMouth(happyOpen);
        putEyes(happyOpen);
        sing(S_happy);
        // putMouth(happyClosed);
        putEyes(happyClosed);
        tiptoeSwing(1,500,20);
        // putMouth(happyOpen);
        putEyes(happyOpen);
        sing(S_superHappy);
        // putMouth(happyClosed);
        putEyes(happyClosed);
        tiptoeSwing(1,500,20); 

        home();  
        // putMouth(happyOpen);
        putEyes(happyOpen);
    break;


    case PandoSad: 
        // putMouth(sad);
        putEyes(sadOpen);
        _moveServos(700, sadPos);     
        bendTones(880, 830, 1.02, 20, 200);
        // putMouth(sadClosed);
        putEyes(sadClosed);
        bendTones(830, 790, 1.02, 20, 200);  
        // putMouth(sadOpen);
        putEyes(sadOpen);
        bendTones(790, 740, 1.02, 20, 200);
        // putMouth(sadClosed);
        putEyes(sadClosed);
        bendTones(740, 700, 1.02, 20, 200);
        // putMouth(sadOpen);
        putEyes(sadOpen);
        bendTones(700, 669, 1.02, 20, 200);
        // putMouth(sad);
        putEyes(sadOpen);
        delay(500);

        home();
        delay(300);
        // putMouth(happyOpen);
        // putEyes(happyOpen);
    break;


    case PandoSleeping:
        _moveServos(700, bedPos);

        putEyes(happyClosed);     

        for(int i=0; i<4;i++){
          // putAnimationMouth(dreamMouth,0);
          bendTones (100, 200, 1.04, 10, 10);
          // putAnimationMouth(dreamMouth,1);
          bendTones (200, 300, 1.04, 10, 10);  
          // putAnimationMouth(dreamMouth,2);
          bendTones (300, 500, 1.04, 10, 10);   
          delay(500);
          // putAnimationMouth(dreamMouth,1);
          bendTones (400, 250, 1.04, 10, 1); 
          // putAnimationMouth(dreamMouth,0);
          bendTones (250, 100, 1.04, 10, 1); 
          delay(500);
        } 

        // putMouth(lineMouth);
        sing(S_cuddly);

        home();  
        // putMouth(happyOpen);
        // putEyes(happyOpen);
    break;


    case PandoFart:
        _moveServos(500,fartPos_1);
        delay(300);     
        // putMouth(lineMouth);
        putEyes(fartLeft);
        sing(S_fart1);  
        // putMouth(tongueOut);
        putEyes(fartRight);
        delay(250);
        _moveServos(500,fartPos_2);
        delay(300);
        // putMouth(lineMouth);
        putEyes(fartLeft);
        sing(S_fart2); 
        // putMouth(tongueOut);
        putEyes(fartRight);
        delay(250);
        _moveServos(500,fartPos_3);
        delay(300);
        // putMouth(lineMouth);
        putEyes(fartLeft);
        sing(S_fart3);
        // putMouth(tongueOut);
        putEyes(fartRight);    
        delay(300);

        home(); 
        delay(500); 
        // putMouth(happyOpen);
        putEyes(happyOpen);
    break;


    case PandoConfused:
        _moveServos(300, confusedPos); 
        // putMouth(confused);
        putEyes(confused);
        sing(S_confused);
        delay(500);

        home();  
        // putMouth(happyOpen);
        // putEyes(happyOpen);
    break;


    case PandoLove:
        // putMouth(heart);
        putEyes(heart);
        sing(S_cuddly);
        crusaito(2,1500,15,1);

        home(); 
        sing(S_happy_short);  
        // putMouth(happyOpen);
        // putEyes(happyOpen);
    break;


    case PandoAngry: 
        _moveServos(300, angryPos); 
        // putMouth(angry);
        putEyes(angry);

        _tone(note_A5,100,30);
        bendTones(note_A5, note_D6, 1.02, 7, 4);
        bendTones(note_D6, note_G6, 1.02, 10, 1);
        bendTones(note_G6, note_A5, 1.02, 10, 1);
        delay(15);
        bendTones(note_A5, note_E5, 1.02, 20, 4);
        delay(400);
        _moveServos(200, headLeft); 
        bendTones(note_A5, note_D6, 1.02, 20, 4);
        _moveServos(200, headRight); 
        bendTones(note_A5, note_E5, 1.02, 20, 4);

        home();  
        // putMouth(happyOpen);
        // putEyes(happyOpen);
    break;


    case PandoFretful: 
        // putMouth(angry);
        putEyes(angry);

        bendTones(note_A5, note_D6, 1.02, 20, 4);
        bendTones(note_A5, note_E5, 1.02, 20, 4);
        delay(300);

        // putMouth(lineMouth);
        // putEyes(happyClosed);


        for(int i=0; i<4; i++){
          _moveServos(100, fretfulPos);   
          home();
        }

        // putMouth(angry);
        // putEyes(angry);

        delay(500);

        home();  
        // putMouth(happyOpen);
        // putEyes(happyOpen);
    break;


    case PandoMagic:

        //Initial note frecuency = 400
        //Final note frecuency = 1000
        
        // Reproduce the animation four times
        for(int i = 0; i<4; i++){ 

          int noteM = 400; 

            for(int index = 0; index<6; index++){
              // putAnimationMouth(adivinawi,index);
              bendTones(noteM, noteM+100, 1.04, 10, 10);    //400 -> 1000 
              noteM+=100;
            }

            // clearMouth();
            bendTones(noteM-100, noteM+100, 1.04, 10, 10);  //900 -> 1100

            for(int index = 0; index<6; index++){
              // putAnimationMouth(adivinawi,index);
              bendTones(noteM, noteM+100, 1.04, 10, 10);    //1000 -> 400 
              noteM-=100;
            }
        } 
 
        delay(300);
        // putMouth(happyOpen);
        putEyes(happyOpen);
    break;


    case PandoWave:
        
        // Reproduce the animation four times
        for(int i = 0; i<2; i++){ 

            int noteW = 500; 

            for(int index = 0; index<10; index++){
              // putAnimationMouth(wave,index);
              bendTones(noteW, noteW+100, 1.02, 10, 10); 
              noteW+=101;
            }
            for(int index = 0; index<10; index++){
              // putAnimationMouth(wave,index);
              bendTones(noteW, noteW+100, 1.02, 10, 10); 
              noteW+=101;
            }
            for(int index = 0; index<10; index++){
              // putAnimationMouth(wave,index);
              bendTones(noteW, noteW-100, 1.02, 10, 10); 
              noteW-=101;
            }
            for(int index = 0; index<10; index++){
              // putAnimationMouth(wave,index);
              bendTones(noteW, noteW-100, 1.02, 10, 10); 
              noteW-=101;
            }
        }    

        // clearMouth();
        delay(100);
        // putMouth(happyOpen);
        putEyes(happyOpen);
    break;

    case PandoVictory:
        
        // putMouth(smallSurprise);
        for (int i = 0; i < 60; ++i){
          int pos[]={90,90,90+i,90-i};  
          _moveServos(10,pos);
          _tone(1600+i*20,15,1);
        }

        // putMouth(bigSurprise);
        for (int i = 0; i < 60; ++i){
          int pos[]={90,90,150-i,30+i};  
          _moveServos(10,pos);
          _tone(2800+i*20,15,1);
        }

        // putMouth(happyOpen);
        // SUPER HAPPY
        //-----
        tiptoeSwing(1,500,20);
        sing(S_superHappy);
        // putMouth(happyClosed);
        tiptoeSwing(1,500,20); 
        //-----

        home();
        // clearMouth();
        // putMouth(happyOpen);
        putEyes(happyOpen);

    break;

    case PandoFail:

        // putMouth(sadOpen);
        _moveServos(300,bendPos_1);
        _tone(900,200,1);
        // putMouth(sadClosed);
        _moveServos(300,bendPos_2);
        _tone(600,200,1);
        // putMouth(confused);
        _moveServos(300,bendPos_3);
        _tone(300,200,1);
        _moveServos(300,bendPos_4);
        // putMouth(xMouth);

        detachServos();
        _tone(150,2200,1);
        
        delay(600);
        // clearMouth();
        // putMouth(happyOpen);
        putEyes(happyOpen);
        home();

    break;

    case PandoThinking:      // PandoFretful:
        // putMouth(angry);
        // putEyes(angry);
        // normalEyes();

        // bendTones(note_A5, note_D6, 1.02, 20, 4);
        // bendTones(note_A5, note_E5, 1.02, 20, 4);
        // delay(50);

        normalEyes();
        _moveServos(50, fretfulPos);   
        home();
        // delay(100);

        normalEyesLeft();
        _moveServos(50, fretfulPos);   
        home();
        // delay(100);

        normalEyesUpLeft();
        _moveServos(50, fretfulPos);   
        home();
        // delay(100);

        normalEyesUp();
        _moveServos(50, fretfulPos);   
        home();
        // delay(100);

        normalEyesUpRight();
        _moveServos(50, fretfulPos);   
        home();
        // delay(100);

        normalEyesRight();
        _moveServos(50, fretfulPos);   
        home();
        // delay(100);

        // putMouth(angry);
        // putEyes(angry);

        // home();  
        // putMouth(happyOpen);
        // putEyes(happyOpen);
    break;

  }
}









///////////////////////////////////////////////////////////////////
//-- EYES ---------------------------------------------------//
///////////////////////////////////////////////////////////////////

// Print information on screen
void Pando::print(const char str[], uint16_t speed) {
  ht1632c.print(str, speed);
}

void Pando::smileEyes() {
  ht1632c.clearScreen();

  ht1632c.setPixel(4, 3);
  ht1632c.setPixel(4, 4);

  ht1632c.setPixel(5, 2);
  ht1632c.setPixel(5, 3);

  ht1632c.setPixel(6, 2);
  ht1632c.setPixel(6, 3);

  ht1632c.setPixel(7, 3);
  ht1632c.setPixel(7, 4);

  ht1632c.setPixel(8, 4);
  ht1632c.setPixel(8, 5);

  ht1632c.setPixel(15, 4);
  ht1632c.setPixel(15, 5);

  ht1632c.setPixel(16, 3);
  ht1632c.setPixel(16, 4);

  ht1632c.setPixel(17, 2);
  ht1632c.setPixel(17, 3);

  ht1632c.setPixel(18, 2);
  ht1632c.setPixel(18, 3);

  ht1632c.setPixel(19, 3);
  ht1632c.setPixel(19, 4);

  ht1632c.writeScreen();
}

void Pando::happyOpenEyes() {
  ht1632c.clearScreen();

  ht1632c.setPixel(4, 3);
  ht1632c.setPixel(4, 4);
  ht1632c.setPixel(4, 5);
  ht1632c.setPixel(4, 6);
  ht1632c.setPixel(5, 2);
  ht1632c.setPixel(6, 2);
  ht1632c.setPixel(7, 2);
  ht1632c.setPixel(8, 3);
  ht1632c.setPixel(8, 4);
  ht1632c.setPixel(8, 5);
  ht1632c.setPixel(8, 6);

  ht1632c.setPixel(15, 3);
  ht1632c.setPixel(15, 4);
  ht1632c.setPixel(15, 5);
  ht1632c.setPixel(15, 6);
  ht1632c.setPixel(16, 2);
  ht1632c.setPixel(17, 2);
  ht1632c.setPixel(18, 2);
  ht1632c.setPixel(19, 3);
  ht1632c.setPixel(19, 4);
  ht1632c.setPixel(19, 5);
  ht1632c.setPixel(19, 6);

  ht1632c.writeScreen(); 
}

void Pando::angryEyes() {
  ht1632c.clearScreen();

  ht1632c.setPixel(4, 1);
  ht1632c.setPixel(4, 2);
  ht1632c.setPixel(4, 3);
  ht1632c.setPixel(4, 4);
  ht1632c.setPixel(5, 2);
  ht1632c.setPixel(5, 3);
  ht1632c.setPixel(5, 4);
  ht1632c.setPixel(5, 5);
  ht1632c.setPixel(6, 3);
  ht1632c.setPixel(6, 4);
  ht1632c.setPixel(6, 5);
  ht1632c.setPixel(6, 6);
  ht1632c.setPixel(7, 4);
  ht1632c.setPixel(7, 5);
  ht1632c.setPixel(7, 6);
  ht1632c.setPixel(8, 5);

  ht1632c.setPixel(15, 5);
  ht1632c.setPixel(16, 4);
  ht1632c.setPixel(16, 5);
  ht1632c.setPixel(16, 6);
  ht1632c.setPixel(17, 3);
  ht1632c.setPixel(17, 4);
  ht1632c.setPixel(17, 5);
  ht1632c.setPixel(17, 6);
  ht1632c.setPixel(18, 2);
  ht1632c.setPixel(18, 3);
  ht1632c.setPixel(18, 4);
  ht1632c.setPixel(18, 5);
  ht1632c.setPixel(19, 1);
  ht1632c.setPixel(19, 2);
  ht1632c.setPixel(19, 3);
  ht1632c.setPixel(19, 4);

  ht1632c.writeScreen(); 
}

void Pando::sadEyes() {
  ht1632c.clearScreen();

  ht1632c.setPixel(3, 4);

  ht1632c.setPixel(4, 5);

  ht1632c.setPixel(5, 5);

  ht1632c.setPixel(6, 5);

  ht1632c.setPixel(7, 5);

  ht1632c.setPixel(8, 4);

  ht1632c.setPixel(15, 4);

  ht1632c.setPixel(16, 5);

  ht1632c.setPixel(17, 5);

  ht1632c.setPixel(18, 5);

  ht1632c.setPixel(19, 5);

  ht1632c.setPixel(20, 4);

  ht1632c.writeScreen(); 
}

void Pando::sadOpenEyes() {
  ht1632c.clearScreen();

  ht1632c.setPixel(4, 4);
  ht1632c.setPixel(4, 5);

  ht1632c.setPixel(5, 3);
  ht1632c.setPixel(5, 4);
  ht1632c.setPixel(5, 5);
  ht1632c.setPixel(5, 6);

  ht1632c.setPixel(6, 3);
  ht1632c.setPixel(6, 4);
  ht1632c.setPixel(6, 5);
  ht1632c.setPixel(6, 6);

  ht1632c.setPixel(7, 2);
  ht1632c.setPixel(7, 3);
  ht1632c.setPixel(7, 4);
  ht1632c.setPixel(7, 5);
  ht1632c.setPixel(7, 6);

  ht1632c.setPixel(8, 2);
  ht1632c.setPixel(8, 3);
  ht1632c.setPixel(8, 4);
  ht1632c.setPixel(8, 5);

  ht1632c.setPixel(9, 3);

  ht1632c.setPixel(15, 3);

  ht1632c.setPixel(16, 2);
  ht1632c.setPixel(16, 3);
  ht1632c.setPixel(16, 4);
  ht1632c.setPixel(16, 5);

  ht1632c.setPixel(17, 2);
  ht1632c.setPixel(17, 3);
  ht1632c.setPixel(17, 4);
  ht1632c.setPixel(17, 5);
  ht1632c.setPixel(17, 6);

  ht1632c.setPixel(18, 3);
  ht1632c.setPixel(18, 4);
  ht1632c.setPixel(18, 5);
  ht1632c.setPixel(18, 6);

  ht1632c.setPixel(19, 3);
  ht1632c.setPixel(19, 4);
  ht1632c.setPixel(19, 5);
  ht1632c.setPixel(19, 6);

  ht1632c.setPixel(20, 4);
  ht1632c.setPixel(20, 5);

  ht1632c.writeScreen(); 
}

void Pando::sadCloseEyes() {
  ht1632c.clearScreen();

  ht1632c.setPixel(5, 6);

  ht1632c.setPixel(6, 5);

  ht1632c.setPixel(7, 4);

  ht1632c.setPixel(8, 3);

  ht1632c.setPixel(15, 3);

  ht1632c.setPixel(16, 4);

  ht1632c.setPixel(17, 5);

  ht1632c.setPixel(18, 6);

  ht1632c.writeScreen(); 
}

void Pando::fartLeftEyes() {
  ht1632c.clearScreen();

  ht1632c.setPixel(3, 4);
  ht1632c.setPixel(3, 5);

  ht1632c.setPixel(4, 3);
  ht1632c.setPixel(4, 6);

  ht1632c.setPixel(5, 3);
  ht1632c.setPixel(5, 6);

  ht1632c.setPixel(6, 3);
  ht1632c.setPixel(6, 6);

  ht1632c.setPixel(7, 3);
  ht1632c.setPixel(7, 6);

  ht1632c.setPixel(8, 4);
  ht1632c.setPixel(8, 5);

  ht1632c.setPixel(15, 4);
  ht1632c.setPixel(15, 5);

  ht1632c.setPixel(16, 3);
  ht1632c.setPixel(16, 6);

  ht1632c.setPixel(17, 3);
  ht1632c.setPixel(17, 6);

  ht1632c.setPixel(18, 3);
  ht1632c.setPixel(18, 6);

  ht1632c.setPixel(19, 3);
  ht1632c.setPixel(19, 6);

  ht1632c.setPixel(20, 4);
  ht1632c.setPixel(20, 5);

  ht1632c.setPixel(4, 4);
  ht1632c.setPixel(5, 4);

  ht1632c.setPixel(16, 4);
  ht1632c.setPixel(17, 4);

  ht1632c.writeScreen(); 
}

void Pando::fartRightEyes() {
  ht1632c.clearScreen();

  ht1632c.setPixel(3, 4);
  ht1632c.setPixel(3, 5);

  ht1632c.setPixel(4, 3);
  ht1632c.setPixel(4, 6);

  ht1632c.setPixel(5, 3);
  ht1632c.setPixel(5, 6);

  ht1632c.setPixel(6, 3);
  ht1632c.setPixel(6, 6);

  ht1632c.setPixel(7, 3);
  ht1632c.setPixel(7, 6);

  ht1632c.setPixel(8, 4);
  ht1632c.setPixel(8, 5);

  ht1632c.setPixel(15, 4);
  ht1632c.setPixel(15, 5);

  ht1632c.setPixel(16, 3);
  ht1632c.setPixel(16, 6);

  ht1632c.setPixel(17, 3);
  ht1632c.setPixel(17, 6);

  ht1632c.setPixel(18, 3);
  ht1632c.setPixel(18, 6);

  ht1632c.setPixel(19, 3);
  ht1632c.setPixel(19, 6);

  ht1632c.setPixel(20, 4);
  ht1632c.setPixel(20, 5);

  ht1632c.setPixel(6, 4);
  ht1632c.setPixel(7, 4);

  ht1632c.setPixel(18, 4);
  ht1632c.setPixel(19, 4);

  ht1632c.writeScreen(); 
}

// check
void Pando::bigEyes() {
  ht1632c.clearScreen();

  ht1632c.setPixel(5, 2);
  ht1632c.setPixel(5, 3);
  ht1632c.setPixel(5, 4);
  ht1632c.setPixel(5, 5);

  ht1632c.setPixel(6, 1);
  ht1632c.setPixel(6, 2);
  ht1632c.setPixel(6, 3);
  ht1632c.setPixel(6, 4);
  ht1632c.setPixel(6, 5);
  ht1632c.setPixel(6, 6);

  ht1632c.setPixel(7, 1);
  ht1632c.setPixel(7, 2);
  ht1632c.setPixel(7, 3);
  ht1632c.setPixel(7, 4);
  ht1632c.setPixel(7, 5);
  ht1632c.setPixel(7, 6);

  ht1632c.setPixel(8, 2);
  ht1632c.setPixel(8, 3);
  ht1632c.setPixel(8, 4);
  ht1632c.setPixel(8, 5);

  ht1632c.setPixel(16, 2);
  ht1632c.setPixel(16, 3);
  ht1632c.setPixel(16, 4);
  ht1632c.setPixel(16, 5);

  ht1632c.setPixel(17, 1);
  ht1632c.setPixel(17, 2);
  ht1632c.setPixel(17, 3);
  ht1632c.setPixel(17, 4);
  ht1632c.setPixel(17, 5);
  ht1632c.setPixel(17, 6);

  ht1632c.setPixel(18, 1);
  ht1632c.setPixel(18, 2);
  ht1632c.setPixel(18, 3);
  ht1632c.setPixel(18, 4);
  ht1632c.setPixel(18, 5);
  ht1632c.setPixel(18, 6);

  ht1632c.setPixel(19, 2);
  ht1632c.setPixel(19, 3);
  ht1632c.setPixel(19, 4);
  ht1632c.setPixel(19, 5);

  ht1632c.writeScreen(); 
}

void Pando::closeEyes() {
  ht1632c.clearScreen();

  ht1632c.setPixel(4, 5);
  ht1632c.setPixel(5, 5);
  ht1632c.setPixel(6, 5);  
  ht1632c.setPixel(7, 5);
  ht1632c.setPixel(8, 5);

  ht1632c.setPixel(16, 5);
  ht1632c.setPixel(17, 5);
  ht1632c.setPixel(18, 5);
  ht1632c.setPixel(19, 5);
  ht1632c.setPixel(20, 5);

  ht1632c.writeScreen(); 
}

void Pando::surpriseEyes() {
  ht1632c.clearScreen();

  ht1632c.setPixel(4, 3);
  ht1632c.setPixel(4, 4);
  ht1632c.setPixel(4, 5);
  ht1632c.setPixel(5, 2);  
  ht1632c.setPixel(5, 6);
  ht1632c.setPixel(6, 2);
  ht1632c.setPixel(6, 4);
  ht1632c.setPixel(6, 6);
  ht1632c.setPixel(7, 2);  
  ht1632c.setPixel(7, 6);  
  ht1632c.setPixel(8, 3);
  ht1632c.setPixel(8, 4);
  ht1632c.setPixel(8, 5);

  ht1632c.setPixel(15, 3);
  ht1632c.setPixel(15, 4);
  ht1632c.setPixel(15, 5);
  ht1632c.setPixel(16, 2);  
  ht1632c.setPixel(16, 6);
  ht1632c.setPixel(17, 2);
  ht1632c.setPixel(17, 4);
  ht1632c.setPixel(17, 6);
  ht1632c.setPixel(18, 2);  
  ht1632c.setPixel(18, 6);
  ht1632c.setPixel(19, 3);
  ht1632c.setPixel(19, 4);
  ht1632c.setPixel(19, 5);

  ht1632c.writeScreen();   
}

void Pando::confusedEyes() {
  ht1632c.clearScreen();

  ht1632c.setPixel(4, 3);
  ht1632c.setPixel(4, 4);
  ht1632c.setPixel(4, 5);
  ht1632c.setPixel(5, 2);  
  ht1632c.setPixel(5, 6);
  ht1632c.setPixel(6, 2);
  ht1632c.setPixel(6, 6);
  ht1632c.setPixel(7, 2);  
  ht1632c.setPixel(7, 6);  
  ht1632c.setPixel(8, 3);
  ht1632c.setPixel(8, 4);
  ht1632c.setPixel(8, 5);

  ht1632c.setPixel(15, 3);
  ht1632c.setPixel(15, 4);
  ht1632c.setPixel(15, 5);
  ht1632c.setPixel(16, 2);  
  ht1632c.setPixel(16, 6);
  ht1632c.setPixel(17, 2);
  ht1632c.setPixel(17, 6);
  ht1632c.setPixel(18, 2);  
  ht1632c.setPixel(18, 6);
  ht1632c.setPixel(19, 3);
  ht1632c.setPixel(19, 4);
  ht1632c.setPixel(19, 5);

  ht1632c.writeScreen();   
}

// check
void Pando::normalEyes() {
  ht1632c.clearScreen();

  // left eye
  ht1632c.setPixel(4, 3);
  ht1632c.setPixel(4, 4);
  ht1632c.setPixel(4, 5);

  ht1632c.setPixel(5, 2);  
  ht1632c.setPixel(5, 6);

  ht1632c.setPixel(6, 2);
  ht1632c.setPixel(6, 6);

  ht1632c.setPixel(7, 2);  
  ht1632c.setPixel(7, 6);

  ht1632c.setPixel(8, 2);
  ht1632c.setPixel(8, 6);

  ht1632c.setPixel(9, 3);
  ht1632c.setPixel(9, 4);
  ht1632c.setPixel(9, 5);

  // right eye
  ht1632c.setPixel(14, 3);
  ht1632c.setPixel(14, 4);
  ht1632c.setPixel(14, 5);

  ht1632c.setPixel(15, 2);  
  ht1632c.setPixel(15, 6);

  ht1632c.setPixel(16, 2);
  ht1632c.setPixel(16, 6);

  ht1632c.setPixel(17, 2);  
  ht1632c.setPixel(17, 6);

  ht1632c.setPixel(18, 2);
  ht1632c.setPixel(18, 6);

  ht1632c.setPixel(19, 3);
  ht1632c.setPixel(19, 4);
  ht1632c.setPixel(19, 5);

  // left eyeball
  ht1632c.setPixel(6, 4);
  ht1632c.setPixel(6, 5);
  ht1632c.setPixel(7, 4);
  ht1632c.setPixel(7, 5);

  // right eyeball
  ht1632c.setPixel(16, 4);
  ht1632c.setPixel(16, 5);
  ht1632c.setPixel(17, 4);
  ht1632c.setPixel(17, 5);

  ht1632c.writeScreen();
}

// check
void Pando::normalEyesLeft() {
  ht1632c.clearScreen();

  // left eye
  ht1632c.setPixel(4, 3);
  ht1632c.setPixel(4, 4);
  ht1632c.setPixel(4, 5);

  ht1632c.setPixel(5, 2);  
  ht1632c.setPixel(5, 6);

  ht1632c.setPixel(6, 2);
  ht1632c.setPixel(6, 6);

  ht1632c.setPixel(7, 2);  
  ht1632c.setPixel(7, 6);

  ht1632c.setPixel(8, 2);
  ht1632c.setPixel(8, 6);

  ht1632c.setPixel(9, 3);
  ht1632c.setPixel(9, 4);
  ht1632c.setPixel(9, 5);

  // right eye
  ht1632c.setPixel(14, 3);
  ht1632c.setPixel(14, 4);
  ht1632c.setPixel(14, 5);

  ht1632c.setPixel(15, 2);  
  ht1632c.setPixel(15, 6);

  ht1632c.setPixel(16, 2);
  ht1632c.setPixel(16, 6);

  ht1632c.setPixel(17, 2);  
  ht1632c.setPixel(17, 6);

  ht1632c.setPixel(18, 2);
  ht1632c.setPixel(18, 6);

  ht1632c.setPixel(19, 3);
  ht1632c.setPixel(19, 4);
  ht1632c.setPixel(19, 5);

  // left eyeball
  ht1632c.setPixel(5, 4);
  ht1632c.setPixel(5, 5);
  ht1632c.setPixel(6, 4);
  ht1632c.setPixel(6, 5);

  // right eyeball
  ht1632c.setPixel(15, 4);
  ht1632c.setPixel(15, 5);
  ht1632c.setPixel(16, 4);
  ht1632c.setPixel(16, 5);

  ht1632c.writeScreen();
}

// check
void Pando::normalEyesRight() {
  ht1632c.clearScreen();

  // left eye
  ht1632c.setPixel(4, 3);
  ht1632c.setPixel(4, 4);
  ht1632c.setPixel(4, 5);

  ht1632c.setPixel(5, 2);  
  ht1632c.setPixel(5, 6);

  ht1632c.setPixel(6, 2);
  ht1632c.setPixel(6, 6);

  ht1632c.setPixel(7, 2);  
  ht1632c.setPixel(7, 6);

  ht1632c.setPixel(8, 2);
  ht1632c.setPixel(8, 6);

  ht1632c.setPixel(9, 3);
  ht1632c.setPixel(9, 4);
  ht1632c.setPixel(9, 5);

  // right eye
  ht1632c.setPixel(14, 3);
  ht1632c.setPixel(14, 4);
  ht1632c.setPixel(14, 5);

  ht1632c.setPixel(15, 2);  
  ht1632c.setPixel(15, 6);

  ht1632c.setPixel(16, 2);
  ht1632c.setPixel(16, 6);

  ht1632c.setPixel(17, 2);  
  ht1632c.setPixel(17, 6);

  ht1632c.setPixel(18, 2);
  ht1632c.setPixel(18, 6);

  ht1632c.setPixel(19, 3);
  ht1632c.setPixel(19, 4);
  ht1632c.setPixel(19, 5);

  // left eyeball
  ht1632c.setPixel(7, 4);
  ht1632c.setPixel(7, 5);
  ht1632c.setPixel(8, 4);
  ht1632c.setPixel(8, 5);

  // right eyeball
  ht1632c.setPixel(17, 4);
  ht1632c.setPixel(17, 5);
  ht1632c.setPixel(18, 4);
  ht1632c.setPixel(18, 5);

  ht1632c.writeScreen();
}

// check
void Pando::normalEyesUp() {
  ht1632c.clearScreen();

  // left eye
  ht1632c.setPixel(4, 3);
  ht1632c.setPixel(4, 4);
  ht1632c.setPixel(4, 5);

  ht1632c.setPixel(5, 2);  
  ht1632c.setPixel(5, 6);

  ht1632c.setPixel(6, 2);
  ht1632c.setPixel(6, 6);

  ht1632c.setPixel(7, 2);  
  ht1632c.setPixel(7, 6);

  ht1632c.setPixel(8, 2);
  ht1632c.setPixel(8, 6);

  ht1632c.setPixel(9, 3);
  ht1632c.setPixel(9, 4);
  ht1632c.setPixel(9, 5);

  // right eye
  ht1632c.setPixel(14, 3);
  ht1632c.setPixel(14, 4);
  ht1632c.setPixel(14, 5);

  ht1632c.setPixel(15, 2);  
  ht1632c.setPixel(15, 6);

  ht1632c.setPixel(16, 2);
  ht1632c.setPixel(16, 6);

  ht1632c.setPixel(17, 2);  
  ht1632c.setPixel(17, 6);

  ht1632c.setPixel(18, 2);
  ht1632c.setPixel(18, 6);

  ht1632c.setPixel(19, 3);
  ht1632c.setPixel(19, 4);
  ht1632c.setPixel(19, 5);

  // left eyeball
  ht1632c.setPixel(6, 3);
  ht1632c.setPixel(6, 4);
  ht1632c.setPixel(7, 3);
  ht1632c.setPixel(7, 4);

  // right eyeball
  ht1632c.setPixel(16, 3);
  ht1632c.setPixel(16, 4);
  ht1632c.setPixel(17, 3);
  ht1632c.setPixel(17, 4);

  ht1632c.writeScreen();
}

// check
void Pando::normalEyesUpLeft() {
  ht1632c.clearScreen();

  // left eye
  ht1632c.setPixel(4, 3);
  ht1632c.setPixel(4, 4);
  ht1632c.setPixel(4, 5);

  ht1632c.setPixel(5, 2);  
  ht1632c.setPixel(5, 6);

  ht1632c.setPixel(6, 2);
  ht1632c.setPixel(6, 6);

  ht1632c.setPixel(7, 2);  
  ht1632c.setPixel(7, 6);

  ht1632c.setPixel(8, 2);
  ht1632c.setPixel(8, 6);

  ht1632c.setPixel(9, 3);
  ht1632c.setPixel(9, 4);
  ht1632c.setPixel(9, 5);

  // right eye
  ht1632c.setPixel(14, 3);
  ht1632c.setPixel(14, 4);
  ht1632c.setPixel(14, 5);

  ht1632c.setPixel(15, 2);  
  ht1632c.setPixel(15, 6);

  ht1632c.setPixel(16, 2);
  ht1632c.setPixel(16, 6);

  ht1632c.setPixel(17, 2);  
  ht1632c.setPixel(17, 6);

  ht1632c.setPixel(18, 2);
  ht1632c.setPixel(18, 6);

  ht1632c.setPixel(19, 3);
  ht1632c.setPixel(19, 4);
  ht1632c.setPixel(19, 5);

  // left eyeball
  ht1632c.setPixel(5, 3);
  ht1632c.setPixel(5, 4);
  ht1632c.setPixel(6, 3);
  ht1632c.setPixel(6, 4);

  // right eyeball
  ht1632c.setPixel(15, 3);
  ht1632c.setPixel(15, 4);
  ht1632c.setPixel(16, 3);
  ht1632c.setPixel(16, 4);

  ht1632c.writeScreen();
}

// check
void Pando::normalEyesUpRight() {
  ht1632c.clearScreen();

  // left eye
  ht1632c.setPixel(4, 3);
  ht1632c.setPixel(4, 4);
  ht1632c.setPixel(4, 5);

  ht1632c.setPixel(5, 2);  
  ht1632c.setPixel(5, 6);

  ht1632c.setPixel(6, 2);
  ht1632c.setPixel(6, 6);

  ht1632c.setPixel(7, 2);  
  ht1632c.setPixel(7, 6);

  ht1632c.setPixel(8, 2);
  ht1632c.setPixel(8, 6);

  ht1632c.setPixel(9, 3);
  ht1632c.setPixel(9, 4);
  ht1632c.setPixel(9, 5);

  // right eye
  ht1632c.setPixel(14, 3);
  ht1632c.setPixel(14, 4);
  ht1632c.setPixel(14, 5);

  ht1632c.setPixel(15, 2);  
  ht1632c.setPixel(15, 6);

  ht1632c.setPixel(16, 2);
  ht1632c.setPixel(16, 6);

  ht1632c.setPixel(17, 2);  
  ht1632c.setPixel(17, 6);

  ht1632c.setPixel(18, 2);
  ht1632c.setPixel(18, 6);

  ht1632c.setPixel(19, 3);
  ht1632c.setPixel(19, 4);
  ht1632c.setPixel(19, 5);

  // left eyeball
  ht1632c.setPixel(7, 3);
  ht1632c.setPixel(7, 4);
  ht1632c.setPixel(8, 3);
  ht1632c.setPixel(8, 4);

  // right eyeball
  ht1632c.setPixel(17, 3);
  ht1632c.setPixel(17, 4);
  ht1632c.setPixel(18, 3);
  ht1632c.setPixel(18, 4);

  ht1632c.writeScreen();
}

void Pando::smallLoveEyes() {
  ht1632c.clearScreen();

  // left eye
  ht1632c.setPixel(4, 3);

  ht1632c.setPixel(5, 2);
  ht1632c.setPixel(5, 3);
  ht1632c.setPixel(5, 4);

  ht1632c.setPixel(6, 3);
  ht1632c.setPixel(6, 4);
  ht1632c.setPixel(6, 5);

  ht1632c.setPixel(7, 2);
  ht1632c.setPixel(7, 3);
  ht1632c.setPixel(7, 4);

  ht1632c.setPixel(8, 3);

  // right eye
  ht1632c.setPixel(15, 3);

  ht1632c.setPixel(16, 2);
  ht1632c.setPixel(16, 3);
  ht1632c.setPixel(16, 4);

  ht1632c.setPixel(17, 3);
  ht1632c.setPixel(17, 4);
  ht1632c.setPixel(17, 5);

  ht1632c.setPixel(18, 2);
  ht1632c.setPixel(18, 3);
  ht1632c.setPixel(18, 4);

  ht1632c.setPixel(19, 3);

  ht1632c.writeScreen();
}

void Pando::loveEyes() {
  ht1632c.clearScreen();

  // left eye
  ht1632c.setPixel(3, 2);
  ht1632c.setPixel(3, 3);

  ht1632c.setPixel(4, 1);
  ht1632c.setPixel(4, 2);
  ht1632c.setPixel(4, 3);
  ht1632c.setPixel(4, 4);

  ht1632c.setPixel(5, 1);
  ht1632c.setPixel(5, 2);
  ht1632c.setPixel(5, 3);
  ht1632c.setPixel(5, 4);
  ht1632c.setPixel(5, 5);

  ht1632c.setPixel(6, 2);
  ht1632c.setPixel(6, 3);
  ht1632c.setPixel(6, 4);
  ht1632c.setPixel(6, 5);
  ht1632c.setPixel(6, 6);

  ht1632c.setPixel(7, 1);
  ht1632c.setPixel(7, 2);
  ht1632c.setPixel(7, 3);
  ht1632c.setPixel(7, 4);
  ht1632c.setPixel(7, 5);

  ht1632c.setPixel(8, 1);
  ht1632c.setPixel(8, 2);
  ht1632c.setPixel(8, 3);
  ht1632c.setPixel(8, 4);

  ht1632c.setPixel(9, 2);
  ht1632c.setPixel(9, 3);

  // right eye
  ht1632c.setPixel(14, 2);
  ht1632c.setPixel(14, 3);

  ht1632c.setPixel(15, 1);
  ht1632c.setPixel(15, 2);
  ht1632c.setPixel(15, 3);
  ht1632c.setPixel(15, 4);

  ht1632c.setPixel(16, 1);
  ht1632c.setPixel(16, 2);
  ht1632c.setPixel(16, 3);
  ht1632c.setPixel(16, 4);
  ht1632c.setPixel(16, 5);

  ht1632c.setPixel(17, 2);
  ht1632c.setPixel(17, 3);
  ht1632c.setPixel(17, 4);
  ht1632c.setPixel(17, 5);
  ht1632c.setPixel(17, 6);

  ht1632c.setPixel(18, 1);
  ht1632c.setPixel(18, 2);
  ht1632c.setPixel(18, 3);
  ht1632c.setPixel(18, 4);
  ht1632c.setPixel(18, 5);

  ht1632c.setPixel(19, 1);
  ht1632c.setPixel(19, 2);
  ht1632c.setPixel(19, 3);
  ht1632c.setPixel(19, 4);

  ht1632c.setPixel(20, 2);
  ht1632c.setPixel(20, 3);

  ht1632c.writeScreen();
}



///////////////////////////////////////////////////////////////////
//-- Eyes Animation --------------------------------------------------//
///////////////////////////////////////////////////////////////////

void Pando::blinkEyes() {
  happyOpenEyes();
  delay(400);
  closeEyes();
  delay(100);
}

void Pando::binkLoveEyes() {
  smallLoveEyes();
  delay(500);
  loveEyes();
  delay(500);
}

void Pando::gazeAround() {
  normalEyes();
  delay(100);
  normalEyesLeft();
  delay(100);
  normalEyesUpLeft();
  delay(100);
  normalEyesUp();
  delay(100);
  normalEyesUpRight();
  delay(100);
  normalEyesRight();
  delay(100);
}


