#include <ioavr.h>
#include <inavr.h>
#include "sys_event.h"

#include <stdio.h>    
#include <stdlib.h>


#define DV_480Hz 15
#define DV_96Hz 5
#define DV_DEMO  40

#define KB_IN PINE
#define KB_OUT PORTE       // Input buttons 
#define KB_CTRL DDRE
#define LED PORTB          // 7-seg display
#define LED_CTRL DDRB
#define LED_DG PORTD       // Digits controller
#define LED_DG_DIR DDRD

#define DEC_OFF 0x3F
#define DISP_MAX 6

#define EVENT_PROCESSED 0


//System events
#define SYS_IDLE    0x00
#define TIME_UPDATE 0x01
#define KB_HIT      0x02


//Decoding table based on CD4056
__flash unsigned char bcd_to_led[] = {
    
        0xDE, 0x82, 0xEC, 0xE6,
	0xB2, 0x76, 0x7E, 0xC2,
	0xFE, 0xF6, 0xFA, 0x3E,
	0x5C, 0xAE, 0x7C, 0x78
};

__flash unsigned char DEC_1_OF_6[] =
    {0xFE, 0xFD, 0xFB, 0xF7, 0xEF, 0xDF};


unsigned char disp[8] = {0,0,0,0,0,0};
unsigned char symb_ID;
unsigned char cur_disp;
unsigned char cur_seg;
unsigned char *result;

volatile TSystemEvent rq;

void InitDevices()
{
    LED_DG = 0xFE;
    LED_DG_DIR = 0xFF;
    LED_CTRL = 0xFF;
    KB_CTRL = 0x00;
    KB_OUT = 0xFF; //Enable Pull-Up on port
    //Prescaler fclk / 1024 = 7200Hz
    //Prescaler for T0 is different than for T1,T2,T3
    OCR0 = DV_480Hz - 1;
    TCCR0 = (1 << WGM01) | (1 << CS02) | (1 << CS01) | (1 << CS00);
    TIMSK = (1 << OCIE0);
    __enable_interrupt();
}

//Global shared variables
__no_init unsigned char kb_rep;
__no_init unsigned char kb_reg;


void hexToDec(char inp[]) {

    int x = 1;
    char *szNumbers = inp;
    char * pEnd;
    long int li1;
    li1 = strtol (szNumbers,&pEnd,16);
    
    for(int i=0; i<6; i++) {
        disp[i] =  bcd_to_led[(li1 / x % 10)];
        x *= 10;
    }
    cur_disp = 0;
    
}
//4 is 12345 / 10 % 10

void kbService()
{

static unsigned char kb_prev;
static unsigned char kb_tmr;
static unsigned char kb_stat;
//Local variables -
unsigned char kb;
unsigned char mask;
unsigned char cnt;

    kb = ~KB_IN;
    cnt = 1; mask = 0x80;
    //Check for valid key combination
    do {
        if(kb & mask) cnt--;
    } while(mask = mask >> 1);
    if(cnt)
    {
        kb_prev = kb;
        return;
    }
    //Compare with previous state
    if(kb != kb_prev)
    {
        kb_prev = kb;
        kb_tmr = 2;
        kb_stat = 0;
        return;
    }
    if(kb_tmr)
    {
        if((--kb_tmr) == 0)
        {
            rq.kb = 1;
            kb_reg = kb;
            //Check for repetition
            if(kb & kb_rep)
            {
                if(kb_stat & 0x01)
                    kb_tmr = KB_TPM_DY; //rep: 2 x sec
                else
                    kb_tmr = KB_REP_DY; //wait: 2 sec
                kb_stat = 0x01;
            }
        }
    }
}

#pragma vector = TIMER0_COMP_vect
__interrupt void T0_COMP_ISR()

{
static unsigned char kb_dv = DV_96Hz;
    
    LED_DG |= DEC_OFF;
    LED = disp[cur_disp];
    LED_DG = DEC_1_OF_6[cur_disp];
    if((++cur_disp) == DISP_MAX) cur_disp = 0;
    

    if((--kb_dv) == 0)
    {
        kb_dv = DV_96Hz;
        kbService();
    }
}

void main()
{
    cur_seg = 0;
    cur_disp = 0;
    symb_ID = 0;
    rq.ev = SYS_IDLE;
    InitDevices();
    kb_rep = 0x0F;
    while(1)
    {
       
        if(rq.kb)
        {
            rq.kb = EVENT_PROCESSED;
            //LED = LED ^ kb;
            if(kb_reg & 0x01) {
                // LED++;
                symb_ID =  (symb_ID + 1) & 0x0F; // & 0x0F -> mod 16
                disp[cur_seg] = bcd_to_led[symb_ID];

            }
            else if(kb_reg & 0x02) {
                // LED--;
                symb_ID =  (symb_ID - 1) & 0x0F; // & 0x0F -> mod 16	
                disp[cur_seg] = bcd_to_led[symb_ID];

            }
            else if(kb_reg & 0x04) cur_seg++;
            else if(kb_reg & 0x08) cur_seg--;
            else if(kb_reg & 0x10) { 

		*result=symb_ID;
	    	hexToDec(result); 

	    }
	   
            
            //Remove notifications
            kb_reg = KB_IDLE;
        }  
        __sleep();
    }
}

