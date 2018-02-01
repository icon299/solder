void receive_t(void){

	if ( state_wire==0 )
	{
	state_wire=1;
	detectPresence();
	return;
	}
	
	if ( state_wire==1 )
	{
	state_wire=2;
	writebyte(0xCC);//SKIP ROM [CCh]
	writebyte(0x44);//CONVERT T [44h]
	time_wire=100;//87;  // time= time_wire/100
	return;
	}
	
	if ( state_wire==2 )
	{
	if (!time_wire) state_wire=3;
	return;
	}

	if ( state_wire==3 )
	{
	state_wire=4;
	detectPresence();
	return;
	}
	
	/*if ( state_wire==4 )
	{
	state_wire=5;
	writebyte(0xCC);//SKIP ROM [CCh]
	writebyte(0xBE);//READ SCRATCHPAD [BEh]
	return;
	}*/	
	if ( state_wire==4 )
	{
	state_wire=0;
	writebyte(0xCC);//SKIP ROM [CCh]
	writebyte(0xBE);//READ SCRATCHPAD [BEh]
	readbyte(temperature);
	readbyte(temperature+1);

	uint16_t t;
	t=( ((unsigned int)(temperature[1]<<8))|temperature[0] ); 
	
	static uint8_t TerrCount;
	static uint16_t Told;
	if ( t> (Told+10*16) ){
		if (++TerrCount>=3){
			TerrCount=0;
			Temperatura=t;
			Told=t;
			}
		}else{
		TerrCount=0;
		Temperatura=t;
		Told=t;	
		}
	
	if ( Temperatura > (uint16_t)(40.5*16) ) 	// тест аварии (неисправность симмистора) во всех состояниях
		{										
		PORT_OUT |= OUT_ALARM; 
		PORT_OUT |= OUT_ALARM_2;
		PORT_OUT&=~OUT_ON_NAGREV;
		power=220; 
		status_nagrev=4;
		}

	//static uint8_t tmp_power=1;
	switch(status_nagrev)
	{
	case(0):
		if	( Temperatura >= porog-4*16 ) {	
			Old_Temperatura=Temperatura;
			//power=120;//выключили т.к. OCR2 max 155 в теории (10mSek)
			tmp_power=70;
			PORT_OUT|=OUT_ON_NAGREV;
			status_nagrev=1;
			}
		else{
			tmp_power=power=1;// максимальная мощность
			PORT_OUT|=OUT_ON_NAGREV;
			}
		
		break;

	case(1):
		if ( Temperatura >= porog ){
			Old_Temperatura=0;
			tmp_power=70;
			power=220;
			PORT_OUT&=~OUT_ON_NAGREV;	
			status_nagrev=2;
			}
			else
			{
			if ( Temperatura > Old_Temperatura+1 )
				{
				if ( Temperatura > Old_Temperatura+2 ) 
					if ( tmp_power<100 ) tmp_power+=5;
				Old_Temperatura=Temperatura;
				power=130;//выключили, но не совсем
				PORT_OUT&=~OUT_ON_NAGREV;
				}
				else
				{
				if (tmp_power>2) tmp_power-=2;
				power=tmp_power;// мощность
				PORT_OUT|=OUT_ON_NAGREV;
				}
			}
		break;
	
	case(2):
		if ( Temperatura < porog ) 	// это состояние не даст накопиться в time_power_off большой паузе
			status_nagrev=3;		// и алгоритм 3 состояния включится с 50% мощностью
			else
		break;

	case(3):
		if ( Temperatura <= porog )
			{
			#define T_NO_POWER	0
			#define NAGREV_OFF	1
			#define T_BOTTOM	2
			#define T_TOP		3
			
			if ( (power_flag==NAGREV_OFF)||(power_flag==T_NO_POWER) ) power=140;
			power_flag=T_BOTTOM;
			uint8_t inc=1;
			if ( Temperatura+1 < porog ) inc=2;
			if ( Temperatura+2 < porog ) inc=5;
			
			if ( Temperatura != porog )
				if (tmp_power>inc) tmp_power-=inc;// увеличение мощности
			
			//power=tmp_power;
			PORT_OUT|=OUT_ON_NAGREV;
			}
		else 
		if ( Temperatura > porog )
			{// уменьшение мощности
			if ( power_flag==T_BOTTOM ){
				if (tmp_power<100)
					power=100;
				else 
				power=tmp_power;
				}
			power_flag=T_TOP;
			
			uint8_t inc=1;
			if ( Temperatura > porog+1 ) {
				power_flag=T_NO_POWER; 
				power=220;
				if ( Temperatura > porog+2 ) {inc=3;}
				}
			
			if (Temperatura>Old_Temperatura) {// уменьшать мощность пока есть рост Т.
				if (tmp_power<145) tmp_power+=inc;
				}
			PORT_OUT&=~OUT_ON_NAGREV;
			}

		Old_Temperatura=Temperatura;
		break;
	case(4):
		if ( Temperatura < porog )
			{
			PORT_OUT &= ~OUT_ALARM;
			PORT_OUT|=OUT_ON_NAGREV;
			power=1;
			}
		else//	if ( Temperatura >= porog )
			{
			PORT_OUT |= OUT_ALARM;
			PORT_OUT&=~OUT_ON_NAGREV;
			power=220;
			}
		break;
	}

	alarm_T=0;
	if ( Temperatura < 35*16 ) alarm_T=1;
	else
	if ( Temperatura > 40*16 ) alarm_T=2;
	
	
//---------------------------------------------------------------	
	}
}


while(1)
	{
	if ( time_flag & _BV(_led) )// 
		{
		led_off();// потушить цифру
		time_flag &= ~_BV(_led);
		time[_led]=70;
		asm("wdr");//сброс сторожевого таймера
	
		skan_key();
		
		display();// вкл. цифру
		if ( ++znmesto==3 )
			{
			znmesto=0;
			data_led();
			}
		receive_t();
		}
	
	
	if ( time_flag & _BV(_01sek) )
		{
		time_flag &= ~_BV(_01sek);
		time[_01sek]=156;//~0,01
		time_wire--;
	
		if (status_nagrev==3)
		{
		if ( power_flag==T_TOP ){
			if ( power<145 ) power++; else {power=220; power_flag=NAGREV_OFF;}
			}
		else
		if ( power_flag==T_BOTTOM ){
				uint8_t p=power;
				if ( p>tmp_power ) p--; 
				else 
				if ( p<tmp_power ) p++; 
				power=p;	
			}
		}
		
		if ( state==3)
		if (--time_blank==0)
			{
			if ( time_flag & _BV(_blank) )
			{// параметр будет виден
				time_flag &= ~_BV(_blank);
				time_blank=50;//0.5sek
			}
			else
			{
				time_flag |= _BV(_blank);
				time_blank=20;
			}
			}

		if ( alarm_T )
			{
			if ( --time_alarm == 0 )
				{
				if ( PORT_OUT & INDIKATION_ALARM ){//время паузы
					PORT_OUT &= ~INDIKATION_ALARM;
					if ( alarm_T == 1 ) time_alarm=200;
						else time_alarm=50;
					}
					else{
					PORT_OUT |= INDIKATION_ALARM;
					time_alarm=50;
					}
				}
			}
			else {time_alarm=1; PORT_OUT &= ~INDIKATION_ALARM;}
		
		}
	}

}
