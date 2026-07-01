/**
* Проект контроллера автомоек. Версия 10 от 2025
* Copyright (C) 2020 Алексей Шихарбеев
* http://samopal.pro
*/
#include "WC_Event.h"

/**
* Конструктор класса TEvent
* @param _delayOn   - Задержка при включении события, мс
* @param _delayOff  - Задержка при выключении события, мс
* @param _handleOn  - Функция вызываемая при включении события
* @param _hansleOff - Функция вызываемая при отключении события
*/
TEvent::TEvent(uint32_t _delayOn, uint32_t _delayOff, void (*_handle)(bool) ){
    DelayOn   = _delayOn;
    DelayOff  = _delayOff;
    Handle    = _handle;
//    HandleOff = _handleOff;
//    EventOn   = _eventOn;
//    EventOff  = _eventOff;
    State     = ES_NONE;
    SaveState = ES_NONE;
    Type      = ET_NORMAL;
    TimeOn    = 0;
    TimeOff   = 0;
    isOn      = false;
    isChangeState = false;
}

/**
* Изменение типа события
* @param _type      - Установка типа события
* @param _timeOn    - Длительность импульса события, мс (если PULSE, PILSE_OFF, PULSE2, PWM)
* @param _timeOff   - Длительность между импульсами, мс (если PWM)
*/
void TEvent::setType( TEVENT_TYPE_t _type, uint32_t _timeOn, uint32_t _timeOff ){
   Type       = _type;
   TimeOn     = _timeOn;
   TimeOff    = _timeOff;
/*
   Serial.print("!!!!! Relay2 type=");
   Serial.print(Type); 
   Serial.print(" t1=");
   Serial.print(TimeOn);
   Serial.print(" t2=");
   Serial.println(TimeOff);
*/
}


bool TEvent::changeState(){
   bool ret = isChangeState;
   isChangeState = false;
   return ret;
}

/**
* Включение события
*/
void TEvent::on(){
    if( Type == ET_DISABLE )return;
    if( State == ES_WAIT_ON || State == ES_ON )return;
    msOn = millis();
    if( DelayOn == 0 ){
       State = ES_ON;
       setOn();
    }
    else {
       State = ES_WAIT_ON;
    }
}

void TEvent::on( uint32_t _delay ){
   DelayOn = _delay;
   on();
}

/*
* Выключение события
*/
void TEvent::off(){
    if( Type == ET_DISABLE )return;
    if( State == ES_WAIT_OFF || State == ES_OFF )return;
    msOff = millis();
    if( DelayOff == 0 ){
       State = ES_OFF;
       setOff();
    }
    else {
       State = ES_WAIT_OFF;
    }
}

void TEvent::off( uint32_t _delay ){
   DelayOff = _delay;
   off();
}


void TEvent::reset(){
    State = ES_NONE;
}


void TEvent::setOn(){
   if( Handle != NULL )Handle(true);
   isOn = true;
}

void TEvent::setOff(){
   if( Handle != NULL && isOn )Handle(false);
   isOn = false;
}



/*
* Метод loop()
*/
TEVENT_STATUS_t TEvent::loop(){
   uint32_t _ms = millis();
   switch( State ){
       case ES_WAIT_ON :
          if( _ms - msOn > DelayOn ){
             State = ES_ON;
             msOn  = _ms;
             setOn();
          }
          break;  
       case ES_WAIT_OFF :   
          if( _ms - msOn > DelayOff ){
             State = ES_OFF;
             msOn  = _ms;
             setOff();
          }
          break; 
       case ES_ON :
          if( ( Type == ET_PULSE || Type == ET_PULSE2 || Type == ET_PWM )&&isOn&&( _ms - msOn > TimeOn)  ){
             msOn = _ms;
             setOff();
          }
          else if( (Type == ET_PWM )&&isOn==false&&( _ms - msOn > TimeOff) ){
             msOn = _ms;
             setOn();
          }
          break;
       case ES_OFF :
          if( ( Type == ET_PULSE_OFF  )&&!isOn  ){
             msOn = _ms;
             setOn();
          }      
          else if( ( Type == ET_PULSE2  )&&isOn==false&&( _ms - msOn <=TimeOff)  ){
             setOn();
          }      
          else if( (Type == ET_PULSE2 )&&isOn&&( _ms - msOn > TimeOff) ){
             setOff();
          }

          else if( ( Type == ET_PULSE_OFF  )&&isOn&&( _ms - msOn > TimeOn)  ){
             msOn = _ms;
             setOff();
          }
          break;
   }
   if( SaveState != State ){
       isChangeState = true;
       SaveState     = State;
   }
   return State;
 // Serial.printf("!!! Type=%d State=%d\n",(int)Type,(int)State);
}

void TEvent::copyTo(TEvent *dist){
    dist->Type     = Type;
    dist->DelayOn  = DelayOn;
    dist->DelayOff = DelayOff;
    dist->TimeOn   = TimeOn;
    dist->TimeOff  = TimeOff;
    dist->Handle   = Handle;
}


//***********************************************************************************************************************************************************************************
// Класс TEventRGB
//***********************************************************************************************************************************************************************************
TEventRGB::TEventRGB( uint8_t _pin, int _num, int _first ){

   Strip = new Adafruit_NeoPixel(_num, _pin, NEO_GRB + NEO_KHZ800);
   Num           = _num;
   First         = _first;
   TimerOn      = 0;
   TimerOff     = 0;
   Color1       = COLOR_NONE;
   Color2       = COLOR_NONE;
   ColorBlink1  = COLOR_NONE;
   ColorBlink2  = COLOR_NONE;
   ColorMP3     = COLOR_NONE;
   State        = ES_NONE;
   msOn         = 0;
   msOff        = 0;
   isRainbow    = false;
   isMP3        = false;
   HueRainbow   = 0;
   IncRainbow   = 65536/Num*2;
   isBlink0     = false;
   BlinkCount   = 0;
   TimerRainbow = 0;
   NumAp        = LED_STAT_AP1;
   NumSta       = LED_STAT_STA1;
}


void TEventRGB::setColor(uint32_t _color1, uint32_t _color2){
   for( int i=0; i<Num; i++){
       if( First>0 && (i == NumAp || i == NumSta) )continue;
       if( i%2 ){
          if( _color1 != COLOR_NONE )Strip->setPixelColor(i,_color1);
       } 
       else {
          if( _color2 != COLOR_NONE )Strip->setPixelColor(i,_color2);
       }
   }
   Strip->show();
}

void TEventRGB::setColor0(uint32_t _color, bool _blink){
   Color0   = _color;
   isBlink0 = _blink;
   Strip->setPixelColor(NumAp,setBrightness(_color,LED_STAT_BRIGHTNESS));
   Strip->show();
}

void TEventRGB::setColor1(uint32_t _color){
   Strip->setPixelColor(NumSta,setBrightness(_color,LED_STAT_BRIGHTNESS));
   Strip->show();
}

void TEventRGB::set(uint32_t _color1, uint32_t _color2, uint32_t _color11, uint32_t _color12, uint32_t _timer_on, uint32_t _timer_off ){
   Serial.printf("!!! SetRGB #%lX #%lX #%lX #%lX %ld %ld\n", _color1, _color2, _color11, _color12, _timer_on, _timer_off );
   TimerOn      = _timer_on;
   TimerOff     = _timer_off;
   Color1       = _color1;
   Color2       = _color2;
   ColorBlink1  = _color11;
   ColorBlink2  = _color12;
//   Status       = ES_ON;
   setColor(Color1, Color2);
   msRainbowOn  = 0;
   msOn         = millis();
   msOff        = 0;
}

void TEventRGB::setMP3( uint32_t _color ){
    ColorMP3 = _color;
    Serial.printf("!!! SetRGB MP3 #%lX\n",_color);
    if( ColorMP3 == COLOR_NONE ){
       isMP3 = false;
       setColor(Color1, Color2);
    }
    else isMP3 = true;
}

void TEventRGB::setRainbow( bool _flag, uint32_t _timer ){
   Serial.printf("!!! SetRGB Rainbow %d\n",_flag);
   isRainbow = _flag;
   if( isRainbow ){
      TimerRainbow = _timer;
      msRainbowOn  = millis();
      HueRainbow   = 0;   
   }
}

void TEventRGB::setBrightness( int br){
   if( br < 0)Strip->setBrightness(0);
   else if( br >= 10 )Strip->setBrightness(255);
   else Strip->setBrightness(br*25);
}

TEVENT_RGB_TYPE_t  TEventRGB::loop(){
   uint32_t _ms = millis();
// Эффект радуги
   if( isRainbow ){
//      Serial.printf("!!! Rainbow !!! %d %d\n",(int)TimerRainbow,(int)(_ms-msRainbowOn));
      if( TimerRainbow >0 && _ms-msRainbowOn > TimerRainbow ){
          setRainbow(false);
          setColor(Color1, Color2);
          msOff       = 0;
          msOn        = _ms;
          msRainbowOn = 0;
          return ERT_RAINBOW;
//          msOn = millis();
      }
      uint16_t h =  HueRainbow;
      for( int i=0; i<Num; i++){
          if( First>0 && (i == NumAp || i == NumSta) )continue;
          h += IncRainbow;
          Strip->setPixelColor(i, Strip->ColorHSV(h, 255, 100)); 
      }
      Strip->show();
      HueRainbow += IncRainbow;
      return ERT_RAINBOW;
   }
// Мигаем первым светодиодом с частотой Loop()

   if( isBlink0 ){
       if( BlinkCount%2 )Strip->setPixelColor(NumAp,setBrightness(Color0,LED_STAT_BRIGHTNESS));
       else Strip->setPixelColor(NumAp,setBrightness(COLOR_BLACK,LED_STAT_BRIGHTNESS));
       Strip->show();
       BlinkCount++;
   }

// Мигаем ColorMP3 с частотой TIMER_MP3
   if( isMP3 && ColorMP3 != COLOR_NONE){
      if( msOn > 0 && (msOn > _ms || _ms - msOn > TIMER_MP3 )){
         msOn  = 0;
         msOff = _ms;
         setColor(ColorMP3,ColorMP3);
      }
      if( msOff > 0 && (msOff > _ms || _ms - msOff > TIMER_MP3 )){
         msOff  = 0;
         msOn = _ms;
         setColor(Color1,Color2);
      }
      return ERT_MP3;
   }
// Мигаем штатно
   if( TimerOn>0  ){
      if( msOn > 0 && (msOn > _ms || _ms - msOn > TimerOn )){
         msOn  = 0;
         msOff = _ms;
         setColor(ColorBlink1,ColorBlink2);
      }
      if( msOff > 0 && (msOff > _ms || _ms - msOff > TimerOff )){
         msOff  = 0;
         msOn = _ms;
         setColor(Color1,Color1);
      }
      return ERT_BLINK;
   }
   return ERT_NORMAL;
   
}


void TEventRGB::copyTo( TEventRGB *dist ){
   dist->Color1      = Color1;
   dist->Color2      = Color2;
   dist->ColorBlink1 = ColorBlink1;
   dist->ColorBlink2 = ColorBlink2;
   dist->ColorMP3    = ColorMP3;
   dist->TimerOn     = TimerOn;
   dist->TimerOff    = TimerOff;
}

void TEventRGB::set( TEventRGB *src ){
   set(src->Color1, src->Color2, src->ColorBlink1, src->ColorBlink2, src->TimerOn, src->TimerOff);
}

uint32_t TEventRGB::setBrightness(uint32_t color, uint8_t level)
{
    if (level > 10) level = 10;

    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;

    r = (r * level) / 10;
    g = (g * level) / 10;
    b = (b * level) / 10;

    return ((uint32_t)r << 16) |
           ((uint32_t)g << 8)  |
           b;
}


void TEventRGB::setNumAp(int _ap ){
   if( NumAp == _ap  )return;
   if( _ap >= 0 && _ap < Num ){
      uint32_t _color1 = Strip->getPixelColor(NumAp);
      uint32_t _color2 = Strip->getPixelColor(_ap);
      Strip->setPixelColor(NumAp, _color2 );
      Strip->setPixelColor(_ap,   _color1 );
      NumAp = _ap;
   } 
   Strip->show();   
}

void TEventRGB::setNumSta( int _sta ){
   if(  NumSta == _sta )return;
   if( _sta >= 0 && _sta < Num ){
      uint32_t _color1 = Strip->getPixelColor(NumSta);
      uint32_t _color2 = Strip->getPixelColor(_sta);
      Strip->setPixelColor(NumSta, _color2 );
      Strip->setPixelColor(_sta,   _color1);
      NumSta = _sta;
   } 
   Strip->show();
    
}

//***********************************************************************************************************************************************************************************
// Класс TEventMP3
//***********************************************************************************************************************************************************************************
TEventMP3::TEventMP3(Stream &_stream, TEVENT_MP3_GPIO_t _gpio, int _pin, void (*_handle)(bool) ){
   Player = new DFRobotDFPlayerMini();   
   SerialPlayer = &_stream;
   Handle       = _handle;
   DelayOn      = 0;
   TimerOn      = 0;
   Dir          = 0;
   Track        = 0;
   State        = ES_NONE;
   msOn         = 0;
   isPlayer     = false;
   isLoop       = false;
   Color        = COLOR_NONE;
   ColorEvent   = new TEvent(0,0,_handle);
   Priority     = DAFAULT_PRIORITY_MP3;
   GPIO         = _gpio;
   PIN          = _pin;
   isLowGpio    = false;
   isHighGpio   = false;
   init(); 
}

void TEventMP3::init(){
    if( !isPlayer ){
      Player->begin(*SerialPlayer, /*isACK = */true, /*doReset = */true);
      if( Player->readVolume() >= 0 ){
         isPlayer = true;
         if(isPlayer)Player->setTimeOut(500);
         if(isPlayer)Player->outputDevice(DFPLAYER_DEVICE_SD); 

         if( GPIO != ESM_NONE && PIN >= 0 ){
            pinMode(PIN,INPUT);
            if( digitalRead(PIN) )isHighGpio = true;
            Serial.printf("!!! MP3 GPIO %d\n",(int)digitalRead(PIN));
         }
         Serial.printf("!!! MP3 init %d\n",(int)isPlayer);

      }      
    }
}

void TEventMP3::setVolume(int _volume){
    if(isPlayer)Player->volume(_volume);
}

void TEventMP3::setColor(uint32_t _color, uint32_t _timer){
    Color      = _color;
    ColorTimer = _timer;
    if( _timer == 0 )ColorEvent->setType(ET_NORMAL);
    else ColorEvent->setType(ET_PULSE, _timer);
}

void TEventMP3::start(int _dir, int _track,int _priority, uint32_t _delay, uint32_t _timer, bool _is_wait, bool _loop ){   
    Serial.printf("!!! MP3 check %d %d %d\n", _priority, Priority, State);

    if( _priority < Priority && (State == ES_WAIT_ON || State == ES_ON) )return;
    if(!isPlayer)return;

//    if( State != ES_NONE && _priority >= Priority  )return;
    msOn       = millis();
    Dir        = _dir;
    Track      = _track;
    DelayOn    = _delay;
    TimerOn    = _timer;
    waitPlayer = _is_wait;
    Priority   = _priority;
    isLoop     = _loop;
    if( DelayOn == 0 ){
       State = ES_ON;
       _start();
    }
    else {
       _stop();
       State = ES_WAIT_ON;
    }    
    Serial.printf("!!! MP3 start %02d/%03d.mp3 %ld %ld %d %d\n", Dir, Track, DelayOn, TimerOn, (int)waitPlayer, (int)isLoop);
}

void TEventMP3::stop(){
   if( State == ES_NONE )return;
   _stop();
}


void TEventMP3::_start(){
   init();
   if(isPlayer)Player->playFolder(Dir,Track); 
   if( Handle != NULL )ColorEvent->on();
   isOn = true;
   ms1  = 0;
}

void TEventMP3::_stop(){
    Serial.printf("!!! MP3 stop %d/%d.mp3\n", Dir, Track);
   if(isPlayer)Player->stop();
   if( Handle != NULL && isOn )ColorEvent->off();
   State = ES_NONE;   
   isOn = false;
}

void TEventMP3::_replay(uint32_t _delay){
    msOn       = millis();
    if( DelayOn == 0 ){
       State = ES_ON;
       _start();
    }
    else {
       State = ES_WAIT_ON;
    }    
}

int TEventMP3::state(){
   int _state = -1;
   int _flag = 0;
   if( GPIO == ESM_NONE || PIN < 0 ){
      Player->readState();
      _state = Player->readState();
   }
   else if( GPIO == ESM_ENABLE ){
      if( digitalRead(PIN) )_state = 0;
      else _state = 1;
      _flag = 1;
   }
   else {
      if( isLowGpio && isHighGpio ){
         if( digitalRead(PIN) )_state = 0;
         else _state = 1;
         _flag = 1;
      }
      else {
         Player->readState();
         _state = Player->readState();
         if( _state == 1 )isLowGpio = true;
         else isHighGpio = true;
      }
   }
   Serial.printf("!!! MP3 state=%d flag=%d low=%d high=%d\n",_state,_flag,(int)isLowGpio, (int)isHighGpio);
   return _state;
}

/*
* Метод loop()
*/
TEVENT_STATUS_t TEventMP3::loop(){
   uint32_t _ms = millis();
   
   int _state = -1;
   switch( State ){
       case ES_WAIT_ON :
          if( _ms - msOn > DelayOn ){
             State = ES_ON;
             msOn  = _ms;
             _start();
          }
          break;  
       case ES_ON :
          if( !isPlayer )_stop();
          if( TimerOn > 0 && isOn && ( _ms - msOn > TimerOn) ){
             msOn = 0;
             if( isLoop )_replay(2000);
             else stop(); 
          }
          if( isOn && waitPlayer && ( ms1 == 0 || ms1 > _ms || _ms-ms1 > 2000 )){
             ms1 = _ms;
             if(isPlayer){
//                Player->readState();
//                _state = Player->readState();
                _state = state();
                Serial.printf("!!! MP3 state %d ",_state);
                Serial.print((int)digitalRead(PIN));
                Serial.println();

             }
             else {
                _state = 0;
                Serial.printf("!!! MP3 not init\n");
             }
             if( _state <= 0 ){
                
                if( isLoop )_replay(2000);
                else _stop(); 
             }
          }
          break;
   }
//   if( SaveState != State ){
//       isChangeState = true;
//       SaveState     = State;
//   }
   return State;
 // Serial.printf("!!! Type=%d State=%d\n",(int)Type,(int)State);
}





/*   
void TEventMP3::setSound(int _dir, int _sound, bool _loop, uint32_t _color, uint32_t _tm){
    Dir     = _dir;
    Sound   = _sound;
    Loop    = _loop;
    Color   = _color;
    ColorTM = _tm;
    Serial.printf("!!! Set MP3 %d %d %d\n",Dir,Sound,(int)Loop);
}

void TEventMP3::copyTo(TEventMP3 *dist){
    TEvent::copyTo(dist);
    dist->setSound(Dir,Sound,Loop,Color,ColorTM);
}
*/

//***********************************************************************************************************************************************************************************
// Класс TSaveRGB
//***********************************************************************************************************************************************************************************
/*
TSaveRGB::TSaveRGB( TEventRGB *_event, int _id){
    EventRGB = _event;
    ID       = _id;
    Clear();
}

void TSaveRGB::Clear(){
    Count = 0; 
}

int TSaveRGB::Push(int _id ){
    if( Count >= MAX_RGB_STACK_ITEMS )return -1;
    Serial.printf("!!! Save push %d %d #%lX #%lX\n",_id,EventRGB->Type,EventRGB->Color1,EventRGB->Color2);

    StackRGB[Count].ID      = _id;
    StackRGB[Count].Type    = EventRGB->Type;
    StackRGB[Count].TimeOn  = EventRGB->TimeOn;
    StackRGB[Count].TimeOff = EventRGB->TimeOff;
    StackRGB[Count].Color1  = EventRGB->Color1;
    StackRGB[Count].Color2  = EventRGB->Color2;
    StackRGB[Count].Flag    = false;
    Count++;
    return Count; 
}

int TSaveRGB::Pop(){
    if( Count == 0 )return -1;
    Count--;
    EventRGB->setType(StackRGB[Count].Type,StackRGB[Count].TimeOn,StackRGB[Count].TimeOff);
    EventRGB->setColor(StackRGB[Count].Color1,StackRGB[Count].Color2);
    return Count; 
}

int TSaveRGB::Save(int _id, TEVENT_TYPE_t _type, uint32_t _timeOn, uint32_t _timeOff, uint32_t _color1, uint32_t _color2 ){
    Serial.printf("!!! SaveRGB%d Save %d\n",ID,_id);
    if( Count >= MAX_RGB_STACK_ITEMS )return -1;
    int ret = Push( _id );
    EventRGB->setType( _type, _timeOn, _timeOff );
    EventRGB->setColor(_color1, _color2);    
    EventRGB->reset();
    EventRGB->on();
    return ret;
}

int TSaveRGB::Restore( int _id ){
    Serial.printf("!!! SaveRGB%d Restore %d\n",ID,_id);
    if( Count <= 0 )return 1;
    int n = -1;
    for( int i=0; i<Count; i++){
       if( StackRGB[i].ID == _id ){n = i; break; }
    }
//    Serial.printf("!!! SaveRGB Restore2 %d\n",_id);
    if( n < 0 )return -1;
//    Serial.printf("!!! SaveRGB Restore3 %d\n",_id);
    if( n != Count-1 ){
       StackRGB[n].Flag = false;
       return -1;
    }
//    Serial.printf("!!! SaveRGB Restore4 %d\n",_id);
    Pop();
    for( int i=Count-1; i>=0; i-- ){
       if( StackRGB[i].Flag == false )Pop();
    }
    EventRGB->reset();
    EventRGB->on();
    return Count;
}
*/