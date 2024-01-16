/*
 * Museum_Erdgeschichte.c
 *
 * Created: 13.12.2023 14:05:30
 * Author : nassnode
 */ 
// Einbinden der Bibliothek
#include "hd44780.h" //LCD
#include "i2c_master.h" //I2C
#include "pcf8574.h" // PCF8574 I/O Expander Bibliothek
#include "uart.h"  // uart 
#include "adc32.h" // 32-Bit ADC Bibliothek
#include "mcp4725.h" // MCP4725 DAC Bibliothek

// Einbinden von Standardbibliotheken
#include <time.h> // Zeitfunktionen
#include <avr/io.h> // I/O Funktionen für AVR Mikrocontroller
#include <util/delay.h> // Verzögerungsfunktionen


// Deklarieren und initialisieren
//////////////////////////////////////////////////////////////////////////
volatile uint8_t buttons = 0xff;
volatile uint32_t counter = 0; // counter zum hochzählen um das menü zu verlassen
volatile int i1=0,i2 = 0,ledstatus,abfrage = 1,zeithochzaehlen = 0,anzeige = 0 ,ledgruen = 0b01111111, ledgelb = 0b10111111, ledrot = 0b11011111, ledP4 = 0b11110111; // Leds werden durch ein low geschaltet. Ledstatus ist der rückgabevariable. i1 ein übergabevariable
volatile double adctemp= 0.0, adchumid =0.0; 
char timeString[30], timeString1[30]; time_t rawtime[8],wptnow;
time_t rawtime1[8],wptnow1;
//////////////////////////////////////////////////////////////////////////
 
void einlesentempandhumid()
{
	adchumid = adc_readvoltage(0);
	
	_delay_ms(10);
	
	adctemp = adc_readvoltage(1);
}
//////////////////////////////////////////////////////////////////////////
int checkhumidandtemp(double x, double y) // Abfrage der Humid und Temperatur
{
	
	//für das erste wertepaar
	struct tm wp,wp1; // Struktur für aktuelle Zeit 
	wp.tm_hour = 04; // 12:00:00 
	wp.tm_min = 59;
	wp.tm_sec = 59;
	wp1.tm_hour = 13; // 12:00:00
	wp1.tm_min = 21;
	wp1.tm_sec = 31;
	
	
	if(((x >= 20)&&(x<= 22))&&((y>= 40)&&(y<= 60))) // Abfrage für den optimalen Bereich 
	{ 
		
		ledstatus = ledgruen; // led wird grün 
		
	}
	
    else if(((((x < 20) && (x >= 16)) || ((x > 22) && (x <= 26))) && ((y >= 30) && (y <= 70))) || ((((y < 40) && (y >= 30)) || ((y > 60) && (y <= 70))) && ((x < 26) && (x > 16)))) // Abfrage für den toleriert Bereich
	{
		
		if(ledstatus == ledrot) // abfrage ob die LED davor rot war damit er dann in den zweiten Messwert speichern tut
		{
			
			 wptnow1 = mktime(&wp1); // funktion formt das struct in ein UTC zeit um und schreibt das in wptnow1 (time_t)
			 
			 for(int i = 4; i >=1;i--) // Verschiebt das array um eins damit die werte aktualisiert werden.  
			 {
				 
				 rawtime1[i]=rawtime1[i-1];
				 
			 }
			 
			 rawtime1[0]= wptnow1; // beschreibt das erste feld 
			 
			 anzeige = 1;
			 _delay_ms(500);
		}
		
		 ledstatus = ledgelb;  // led wird gelb
	} 
	
	else if (((x<16)||(y<30)||(x>26)||(y>70)) && (ledstatus == ledgelb)) // Abfrage für den roten Bereich und den überschritt in den roten bereich durch abfrage  ledstatus 
	{
		
		wptnow = mktime(&wp);
		
		for(int i = 4; i >=1;i--)
		{
			
			rawtime[i]=rawtime[i-1];
			
		}
		
		rawtime[0]= wptnow;
		
		ledstatus = ledrot; // led wird rot
		
	}
	
	else if((x <= 16)||(x >= 26))
	{
		
		ledstatus = ledstatus & ledP4;
				
	}
	else
	{ 	
		
		ledstatus= ledrot; // wen nichts stimmt soll die led rot werden überprüfen bzw. Erschuetterung ist 
		
	} 
	return ledstatus; // rückgabewert
	
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
void checkVibration()
{
	while (!(PIND & (1 << PD7)))
	{
		
		ledstatus = ledrot;
		
		lcd_init();
		
		lcd_print("Achtung");
		
		lcd_setCursor(1, 0);
		
		lcd_print("Erschuetterung !");
		
		_delay_ms(200);
		
	}
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
void displayData() 
{
	lcd_init();
	
	lcd_print("Temp:");
	
	lcd_setCursor(0, 10);
	
	lcd_printDouble(adctemp, 1);
	
	lcd_printChar(DEGREE);
	
	lcd_print("C");
	
	lcd_setCursor(1, 0);
	
	lcd_print("Humid:");
	
	lcd_setCursor(1, 10);
	
	lcd_printDouble(adchumid, 1);
	
	lcd_printChar(PERCENT);
	
	if(anzeige == 1)
	{
		lcd_print("!");
	}
	
	_delay_ms(200);
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
void buttonCheck(uint32_t button) 
{
	switch (button)
	{
		
		case 0b11111110:
		// Wenn der Button-Wert 0b11111110 ist, wird das Programm mit 0 beendet.
		return 0;
		break;

		case 0b11111011:
		// Wenn der Button-Wert 0b11111011 ist, führen wir eine Reihe Funktionen aus.
		
		while (zeithochzaehlen <= 4)
		{
			
			einlesentempandhumid();
			// Formatieren der Zeitstrings
			strftime(timeString, sizeof(timeString), "%H:%M:%S", localtime(&rawtime[zeithochzaehlen]));
			strftime(timeString1, sizeof(timeString1), "%H:%M:%S", localtime(&rawtime1[zeithochzaehlen]));
			
			i2 = checkhumidandtemp(adctemp,adchumid);
			pcf8574_set_outputs(0x21, i2);
			
			// Eingaben von PCF8574 lesen
			buttons = pcf8574_get_inputs(0x20);

			if (buttons == 0b11110111)
			{
				// Wenn die Buttons den Wert 0b11110111 haben, erhöhen wir zeithochzaehlen und setzen den Counter zurück.
				zeithochzaehlen++;
				counter = 0;
				_delay_ms(250);
			}
			else
			{
				
				// Ansonsten initialisieren wir das LCD und zeigen verschiedene Informationen an.
				lcd_init();
				lcd_print("Zeit ");
				lcd_printInt(zeithochzaehlen);
				lcd_print(": ");
				lcd_setCursor(0, 8);
			
				if (counter == 600)
				{
					zeithochzaehlen = 10;
					buttons = 0b11101111;
					abfrage = 0;
				}

				for (int j = 0; j < 8; j++)
				{
					lcd_printChar(timeString[j]);
				}

				lcd_setCursor(1, 8);

				for (int j = 0; j < 8; j++)
				{
					lcd_printChar(timeString1[j]);
				}
			}

			counter++;
			_delay_ms(10);
		}

		while ((buttons != 0b11011111) && (buttons != 0b11101111))
		{
			// Eine weitere Schleife, die läuft, solange die Buttons nicht 0b11011111 oder 0b11101111 sind.
			lcd_init();
			
			buttons = pcf8574_get_inputs(0x20);
			
			lcd_print("Zeitloeschen ?");
			
			lcd_setCursor(1, 0);
			
			lcd_print("Y(S5)/N(S6)");
			
			_delay_ms(100);
		}

		if (buttons == 0b11101111)
		{
			// Wenn die Buttons den Wert 0b11101111 haben, setzen wir zeithochzaehlen zurück und löschen die Zeitarrays.
			
			for (int i = 0; i <= 5; i++)
			{
				rawtime[i] = 0;
				
				rawtime1[i] = 0;
			}
			anzeige = 0,zeithochzaehlen = 0;
		}
		else if (buttons == 0b11011111)
		{
			// Wenn die Buttons den Wert 0b11011111 haben, setzen wir nur zeithochzaehlen zurück.
			anzeige = 0,zeithochzaehlen = 0;
		}

		break;

		default:
		
		// Wenn der Button-Wert keiner der oben genannten ist, führen wir eine Reihe von Operationen aus.
		checkVibration();
		
		einlesentempandhumid();
		
		i1 = checkhumidandtemp(adctemp, adchumid);
		
		pcf8574_set_outputs(0x21, i1);
		
		displayData();
		
		break;
	}

}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
int main(void)
{
	//////////////////////////////////////////////////////////////////////////
	PORTD |= PIND7;	// setzt den Pin des Port D auf high 
	DDRD |= PD7; // PD7 als Ausgang 
	//////////////////////////////////////////////////////////////////////////
	
	adc_init(); // Analog digital COnverter initialiseren 
	
    while (1) 
    {
		buttons = pcf8574_get_inputs(0x20);
		
		buttonCheck(buttons);
	}
	
}
//////////////////////////////////////////////////////////////////////////
