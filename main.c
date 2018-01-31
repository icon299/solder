#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <util/delay.h>
#define ADC_VREF_TYPE 0x00 //опроное напряжение на AREF (2.5V)
//#define ADC_VREF_TYPE 0x40 //опроное напряжение на AVCC (5V)
#define START_PWM  TCCR1B |=  _BV(CS11)// | _BV(CS10);
#define sb(x,y) (x |= (1<<y))
#define cb(x,y) (x&=~(1<<y))
#define K_SOLDER 50;
#define PIN_ENC PINC      /*Контакты входов энкодера*/
#define ENC_0 PB3         /*0-й вход энкодера*/
#define ENC_1 PB4         /*1-й вход энкодера*/
#define BUTTON PC2        /*Вход кнопки*/
#define PORT_BUTTON PORTC /*Порт кнопки*/
#define PIN_BUTTON PINC   /*Контакт кнопки*/
#define ENC_STATE PIN_ENC & (_BV(PC0) | _BV(PC1)); //состояние енкодера
#define TOP_PWM 0x1FF


volatile unsigned char  flag;
volatile unsigned int timeCount, SolderPower;
unsigned int  EncData, realTemp, *a;
unsigned char EncState=0, T_doneBuz, cifra[4], milsCount=0;
EEMEM  uint16_t TempMem;

//--------flags-----------
#define _status    0
#define _wakeup    1
#define _blink    2
#define _writeMem  3
#define _readyToSleep  4
#define _doneBuz 5
//if ( flag & _BV(_status)) check flag
//flag |= _BV(_status); set 1
//flag &= ~_BV(_status); set 0



void Buz(char w) {
  unsigned char i;
  for (i=0;i<w;i++) {
    sb(PORTB,2);
    _delay_ms(5);
    cb(PORTB,2);
    _delay_ms(5);
  }
}

void EncoderScan(void)//Функция обработки энкодера
{
  unsigned char New;
  New = ENC_STATE;//PIND & 0x03; // Берем текущее значение
  if ((New != EncState) && (EncState == 3)) {
      if(New == 1)
        EncData =EncData-5;
      else {
        EncData =EncData+5;
      }
    flag |= (_BV(_blink)); 
    if(EncData > 360) EncData = 360;//Следим, чтобы не выйти за границы верхнего
    if(EncData < 10) EncData = 10;//и нижнего пределов
    a = &EncData;   //Указателю присвоить адрес текущего значения энкодера (устанавливаемой температуры)
    flag |= _BV(_doneBuz);
    //T_doneBuz = 1;
    timeCount =0;
    flag &= ~_BV(_readyToSleep);
  }
  EncState = New;
}


void Display(void) {
static unsigned char numer=0;
unsigned char simbol;

numer++;
if(numer>3){numer=1;};
simbol=cifra[numer];

cb(PORTB,0);
cb(PORTB,7);
cb(PORTB,6);
// uint8_t seg_table[] = {
//   //afbedpcg
//   0b11111010,  // 0 0xFA
//   0b00100010,  // 1 0x22
//   0b10111001,  // 2 0xB9
//   0b10101011,  // 3 0xAB
//   0b01100011,  // 4 0x63
//   0b11001011,  // 5 0xCB
//   0b11011011,  // 6 0xDB
//   0b10100010,  // 7 0xA2
//   0b11111011,  // 8 0xFB
//   0b11101011,  // 9 0xEB
//   0b00000001   // - 0x01
// };
switch(simbol) { 
  case 0x30: PORTD = 0xFA;break; //0
  case 0x32: PORTD = 0xB9;break; //2
  case 0x33: PORTD = 0xAB;break; //3
  case 0x34: PORTD = 0x63;break; //4
  case 0x35: PORTD = 0xCB;break; //5
  case 0x36: PORTD = 0xDB;break; //6
  case 0x37: PORTD = 0xA2;break; //7
  case 0x38: PORTD = 0xFB;break; //8
  case 0x39: PORTD = 0xEB;break; //9 
  case 0x31: PORTD = 0x22;break; //1
  case 0x2D: PORTD = 0x01;break; //-
  case 0xFF: PORTD = 0x00;break;
  default:  PORTD = 0x01;
};

if(numer==cifra[0])sb(PORTD,2);else cb(PORTD,2);

switch(numer)  {
  case 1: sb(PORTB,0);break;
  case 2: sb(PORTB,7);break;
  case 3: sb(PORTB,6);break;
};  
}

ISR (TIMER0_OVF_vect) //Таймер 0 - для отсчёта временных интервалов (переполнение ~32мс)
{
  timeCount++;//Увеличиваем счётчик переполнений
  if(timeCount < 91) {//Если не прошло 3 сек после установки температуры энкодером
    flag |= _BV(_writeMem);
  }
  if(timeCount >= 91) {           //Если прошло 3 сек = 32 мс х 91
    flag &= ~_BV(_writeMem);
    flag &= ~_BV(_wakeup);
    flag &= ~_BV(_blink);
    a = &realTemp;    //Указателю присвоить адрес текущей температуры жала
  }

  if (timeCount == 27000 ) { // 26340
    flag |= _BV(_readyToSleep);
  }

  if(timeCount >= 28000) {//~15 минут 28175
    flag &= ~_BV(_readyToSleep);
    flag &= ~_BV(_status); // set 0 - sleep mode;
    TCCR1B = 0;//ШИМ остановить
    OCR1A =0;
    timeCount = 0;//Обнулить счётчик переполнений
    TCNT0 = 0;
    TCCR0B = 0;
  }
}

ISR (TIMER1_OVF_vect) //Таймер 1 - для обслуживания ШИМа (переполнение ~65 мс)
{
  //int SolderPower;
  //SolderPower = (EncData - realTemp)*K_SOLDER;
  //if(SolderPower > TOP_PWM) SolderPower = TOP_PWM;
  //if(SolderPower < 0) SolderPower = 0;
  // OCR1AH = (char)(SolderPower>>8);
  // OCR1AL = (char)SolderPower;
  //OCR1A = SolderPower;

  // OCR1AH = (char)(SolderPower>>8);
  // OCR1AL = (char)SolderPower;
}

ISR (TIMER2_COMPA_vect) // 1mc;
{  
  if (milsCount++ > 4) {
    Display();
    if ( flag & _BV(_status)) {
      EncoderScan();
    }
    milsCount =0;  
  }
}

unsigned int read_adc(unsigned char adc_input)
{
  ADMUX=adc_input | (ADC_VREF_TYPE & 0xff);
  _delay_us(10);
  ADCSRA|=0x40;
  while ((ADCSRA & 0x10)==0);
  ADCSRA|=0x10;
  return ADCW;
}

int main(void) {
const uint16_t const_THA=210;//303; //к-во шагов АЦП на 100 С для термопары
const uint16_t const_THA0=230; //температура*10 холодного спая для термопары
uint16_t Kpdstr;//=1000;
static uint8_t p=0;//Счётчики для "пипикания" 

uint32_t buf=0, buf_l=0, T_izm_Sum=0;
uint16_t T_izm,T_zad, T_buf;//OCR_1=0,Tsm=20;;
uint16_t adc_count, adc, zap_max=0;
uint8_t Tsm=20, cnt_proh = 0,cnt_proh_sm = 0;
int8_t diffTemp;


  
cli();
// ADC initialization
// ADC Clock frequency: 62,500 kHz
// ADC Voltage Reference: AVCC pin
// ADC Auto Trigger Source: ADC Stopped
// Digital input buffers on ADC0: On, ADC1: On, ADC2: On, ADC3: Off
// ADC4: Off, ADC5: On
DIDR0=0x18;
ADMUX=ADC_VREF_TYPE & 0xff;
ADCSRA=0x87;

PORTB=0x00;
DDRB=0xFF;

PORTD=0x00;
DDRD=0xFF;

// Port C initialization
// Func6=In Func5=In Func4=In Func3=In Func2=In Func1=In Func0=In 
// State6=T State5=T State4=T State3=T State2=P State1=P State0=P 
PORTC=0x07;
DDRC=0x00;

TCCR0A=0x00;
//TCCR0B=0x05;
TCNT0=0x00;
OCR0A=0x00;
OCR0B=0x00;
TIMSK0=0x01;

//TCCR1A |= _BV(WGM10) | _BV(WGM11) | _BV(COM1A1); //10-ти разрядный Phase correct ШИМ, подключен вывод OC1A (неинвентированный ШИМ)
TCCR1A |= _BV(WGM12) | _BV(WGM11) | _BV(COM1A1); // 9-ти разрядный ШИМ, 
//TIMSK1 |= _BV(TOIE1); //Разрешение прерывания при переполнении таймера 1
  
//Настраиваем Timer2 на 1mc ;
OCR2A = 0xF9;
TCCR2B |= (1<<CS20 | 1<<CS21); // 32
TIMSK2 |= (1<<OCIE2A);
TCCR2A |= (1<<WGM21);//сброс при совпадении

OCR1AH = 0;
OCR1AL = 0;

adc_count =100;

cifra[1]=cifra[2]=cifra[3]=0x2D;cifra[0]=0;
_delay_ms(20);
sei();

EncData = eeprom_read_word (&TempMem);
//EncData = eeprom_read_word(0x10);
if ((EncData >350)||(EncData<50)) EncData=280;
Kpdstr = 1000;//eeprom_read_word(&Kpdstr_eep);
//EncData = 280;

a = &EncData;

flag |= _BV(_blink); //мигаем индикатором
flag |= _BV(_status); //status = work;
flag |= _BV(_doneBuz); // пикнуть когда установиться нужная температура

TCCR0B |= _BV(CS02) | _BV(CS00); // 8Mгц/1024 = 7812,5 Гц = 32мс
TCCR1B |=  _BV(CS10); // clk/1

//START_PWM;
Buz(2);

  while (1) {
    if ( flag & _BV(_status)) {//if(status == work) {
      cnt_proh++;
      if (cnt_proh >= 6 && flag & _BV(_blink)) {
        cifra[1]=cifra[2]=cifra[3]= 0xFF;
      } else {
        cifra[1]=(char)(((*a)/100)%10+0x30);
        cifra[2]=(char)(((*a)/10)%10+0x30); 
        cifra[3]=(char)((*a)%10+0x30);
      }

      buf=0;
      for(adc=1;adc<=adc_count;adc++) {
        buf+=read_adc(5);
      }
        
      buf=buf/adc_count;

      buf_l=const_THA0+((long)buf*1000)/const_THA;
      buf_l=buf_l*Kpdstr/1000; //учитываем подстроечный коэффициент
      T_izm=(int)buf_l;  //измеренная температура *10
      T_zad = EncData*10;

      if((T_zad+Tsm)>T_izm) {//смещаем уставку на Tsm С(град*10) вверх, а потом на каждый градус отклонения даем 10%шим
        buf_l=(((long)T_zad+(long)Tsm-(long)T_izm))*TOP_PWM/100;
        buf=(int)buf_l;
        if(buf>zap_max) {   //для плавного включения
          zap_max=zap_max+5;
          if(zap_max>TOP_PWM) {
            zap_max=TOP_PWM;
          }
          buf=zap_max;
        }
        else {
          zap_max = 0;
        }
      }else {
        buf=0;
        zap_max = 0;
      }

      SolderPower = buf;
      //realTemp = buf;
      cli();
      OCR1AH = (char)(SolderPower>>8);
      OCR1AL = (char)SolderPower;
      sei();



      T_izm_Sum = T_izm_Sum + T_izm;
      if(cnt_proh>9) {
        cnt_proh = 0;
        T_buf = T_izm_Sum/100;

        diffTemp = EncData - T_buf;
        if (diffTemp >=-4 && diffTemp <=4 && !(flag & _BV(_doneBuz)))
          realTemp = EncData;
        else   
          realTemp = T_buf;
        

        buf = T_izm_Sum/10;
        if(((T_zad+Tsm)>=buf)&&(buf>=(T_zad+Tsm-100))) { //считаем проходы когда измер температура в диапазоне регулирования 10С
          cnt_proh_sm++; //счетчик проходов для корректир смещения если темпер в диапазоне
           //flg_tch=0;
        } else {
          cnt_proh_sm=0;
               //flg_tch=1; //флаг мигания точки если температура за диапазоном регулирования
        };

        T_izm_Sum = 0;

        if(cnt_proh_sm>=1) {//если температура в пределах была уже 1проходов, то корректируем смещение уставки
          if((buf>T_zad)&&(Tsm>0)){Tsm--;};
          if((buf<T_zad)&&(Tsm<20)){Tsm++;};
          cnt_proh_sm=0;
        };
      }

      if ((realTemp == EncData) && (flag & _BV(_doneBuz))) {
        Buz(3);
        flag &= ~_BV(_doneBuz);
      }
      //cli();
      if(OCR1A > 0) {
        if(cifra[0]==0){cifra[0]=3;}else{cifra[0]=0;}; //Точка мигает - температура в корректировке
      } else cifra[0]=0; //гасим точку - температура в диапазоне         
      //sei();

      if ( flag & _BV(_writeMem)) {
        if(bit_is_clear(PIN_BUTTON, BUTTON))  {
          
          if (!(flag & _BV(_wakeup))) {
            flag &= ~_BV(_writeMem);
            eeprom_update_word(&TempMem,EncData);
            Buz(2);
          }
          _delay_ms(20);
        }
      }
      if ( flag & _BV(_readyToSleep)) {    
        if (p++ == 50) {
          Buz(2);
          p=0;
        }
      }
    } //end of status = work;
    else { //status = sleep

      cifra[1]=cifra[2]=cifra[3]=0x2D;cifra[0]=0;
      //sei();
      if(bit_is_clear(PIN_BUTTON, BUTTON)) {
        _delay_ms(20);
        //START_PWM;
        a = &EncData;
        flag |= _BV(_wakeup);
        flag |= _BV(_status);
        flag |= _BV(_blink);
        TCCR0B |= _BV(CS02) | _BV(CS00);
        TCCR1B |=  _BV(CS10);
        Buz(2);
      } 
    }
  }   
}
