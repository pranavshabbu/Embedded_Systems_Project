/*P0.4 to P0.9 - LCD
P0.22 - Buzzer -> PINSEL1 |= 0;
P0.23 - AD0.0 -> PINSEL1 |= 1<<14;
P1.23 - PWM 1.4 */


#include <lpc17xx.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define RS_CTRL 0x00000100  //P0.8
#define EN_CTRL 0x00000200  //PO.9
#define DT_CTRL 0x000000F0  //PO.4 TO P0.7
#define BUZZER_PIN (1<<22)  // P0.22 for buzzer
#define PARA 116.602
#define PARB -2.769


unsigned long int init_command[] = {0x30,0x30,0x30,0x20,0x28,0x0c,0x06,0x01,0x80};
unsigned long int temp1 = 0, temp2 = 0, i, j, var1, var2;
unsigned char flag1 = 0, flag2 = 0;
unsigned char msg[] = {"QUALITY:"};
unsigned char msg2[] = {"PPM: "};
unsigned char msg3[] = {"AQI: "};
unsigned char buzzer_state = 0;

void delayms(unsigned int milliseconds)
{
	LPC_TIM0->CTCR=0x0;
	LPC_TIM0->PR=2;
	LPC_TIM0->TCR=0x02;
	LPC_TIM0->TCR=0x1;
	while(LPC_TIM0->TC<milliseconds);
	LPC_TIM0->TCR=0;
}

// Buzzer patterns for different AQI levels
void buzzer_pattern_danger(void) {
		int z;
    LPC_GPIO0->FIOSET = BUZZER_PIN;
    delayms(1000000);
		LPC_GPIO0->FIOCLR = BUZZER_PIN;
}

void buzzer_pattern_bad(void) {
	int x;
	LPC_GPIO0->FIOSET = BUZZER_PIN;
	delayms(500000); 
	LPC_GPIO0->FIOCLR = BUZZER_PIN;
}

void lcd_init(void);
void lcd_write(void);
void port_write(void);
void lcd_print_msg(void);
void lcd_print_msg2(void);
void lcd_print_msg3(void);
void lcd_print_aval(void);
void lcd_print_dval(void);
void initpwm(void);
void keyboard_init(void);
void updatepulsewidth(unsigned int);

int main(void) {
    unsigned int i;
    unsigned int mqReading;
    char disp_qual[6];
    char digitalValStr[14]; //aqi value
   
    int a;
    int aqi_mode=0;
		int aqi_value;
	
		int val;
		float res;
		float rzero=76.63; 
		float rload=10.0;
		float ppm;
		float ratio;

		initpwm();

    SystemInit();
    SystemCoreClockUpdate();
   
	 //for switch p2.12
	 //gpio and input
	 LPC_PINCON->PINSEL4 = 0;
	 LPC_GPIO2->FIODIR = 0;
	 
	 
		LPC_PINCON->PINSEL1 = 0; 				 //P0.22 in fn 0
    LPC_PINCON->PINSEL1 |= 1<<14;    //function 1 on P0.23 for AD0
    LPC_SC->PCONP |= (1<<12);        //peripheral power supply for ADC
    
		LPC_GPIO0->FIODIR = DT_CTRL | RS_CTRL | EN_CTRL; 		//LCD
    LPC_GPIO0->FIODIR |= BUZZER_PIN; 	//configure buzzer pin as output
   
    lcd_init();
    lcd_print_msg();
		
    while(1) {
        // ADC Reading
        LPC_ADC->ADCR = (1<<0) | (1<<21) | (1<<24);
        while(((mqReading = LPC_ADC->ADGDR) & 0X80000000) == 0);
        mqReading = LPC_ADC->ADGDR;
        mqReading >>= 4;
        mqReading &= 0x00000FFF;
				
			//for switch
			a=(LPC_GPIO2 -> FIOPIN>>12)&1;
			if(a==0)
				aqi_mode^=1;
			
				val = mqReading; //ADC output
				res = ((4095./(float)val) - 1.)*rload;
				ppm = PARA * pow((res/rzero),-PARB);

			
			if(aqi_mode==0)lcd_print_msg2();
			else if(aqi_mode==1)lcd_print_msg3();
			
			if(aqi_mode==0)
        sprintf(digitalValStr, "%f      ", ppm);
			else if(aqi_mode==1)
        sprintf(digitalValStr, "%f      ", ppm/7);
       
        // AQI Classification and Buzzer Control
        if(ppm >= 2000) {
            strcpy(disp_qual, "DANGER");
						updatepulsewidth(10000);
            buzzer_pattern_danger();  // Rapid beeping for danger

        }
        else if(ppm >500) {
            strcpy(disp_qual, "BAD   ");
						updatepulsewidth(5000);
            buzzer_pattern_bad();     // Single long beep for bad
        }
        else {
            strcpy(disp_qual, "GOOD  ");
						updatepulsewidth(0);
        }
       
        // LCD Display Update
        temp1 = 0x89;
        flag1 = 0;
        lcd_write();
        delayms(800);
       
        i = 0;
        flag1 = 1;
        while(disp_qual[i] != '\0') {
            temp1 = disp_qual[i];
            lcd_write();
            i += 1;
        }
       
        temp1 = 0xC5;
        flag1 = 0;
        lcd_write();
        delayms(800);
       
        i = 0;
        flag1 = 1;
        while(digitalValStr[i] != '\0') {
            temp1 = digitalValStr[i];
            lcd_write();
            i += 1;
        }
       
        delayms(1000);  // Delay between readings
    }
}

void lcd_init(void) {
    unsigned int x;
    flag1 = 0;
    for(x = 0; x < 9; x++) {
        temp1 = init_command[x];
        lcd_write();
    }
    flag1 = 1;
}

void lcd_write(void) {
    flag2 = (flag1 == 1) ? 0 : ((temp1 == 0x30) || (temp1 == 0x20)) ? 1 : 0;
    temp2 = temp1 & 0xf0;
    port_write();
    if (flag2 == 0) {
        temp2 = temp1 & 0x0f;
        temp2 = temp2 << 4;
        port_write();
    }
}

void port_write(void) {
    LPC_GPIO0->FIOPIN = temp2;
    if (flag1 == 0)
        LPC_GPIO0->FIOCLR = RS_CTRL;
    else
        LPC_GPIO0->FIOSET = RS_CTRL;
    LPC_GPIO0->FIOSET = EN_CTRL;
    delayms(25);
    LPC_GPIO0->FIOCLR = EN_CTRL;
    delayms(300000);
}



void lcd_print_msg(void) {
    unsigned int a;
    for(a = 0; msg[a] != '\0'; a++) {
        temp1 = msg[a];
        lcd_write();
    }
}

void lcd_print_msg2(void) {
    temp1 = 0xC0;
    flag1 = 0;
    lcd_write();
    delayms(800);
    i = 0;
    flag1 = 1;
    while(msg2[i] != '\0') {
        temp1 = msg2[i];
        lcd_write();
        i++;
    }
}
void lcd_print_msg3(void) {
    temp1 = 0xC0;
    flag1 = 0;
    lcd_write();
    delayms(800);
    i = 0;
    flag1 = 1;
    while(msg3[i] != '\0') {
        temp1 = msg3[i];
        lcd_write();
        i++;
    }
}

void initpwm(void)
{
	LPC_PINCON->PINSEL3|=0x8000;
	LPC_PWM1->PCR=0x1000;
	LPC_PWM1->PR=0;
	LPC_PWM1->MR0=10000;
	LPC_PWM1->MCR=2;
	LPC_PWM1->LER=0xff;
	LPC_PWM1->TCR=0x2;
	LPC_PWM1->TCR=0x9;
}

void updatepulsewidth(unsigned int pulsewidth)
{
	LPC_PWM1->MR4=pulsewidth;
	LPC_PWM1->LER=0xff;
}