/******************************************************************************
 *
 *	kaypro.c
 *
 *	Machine interface for Kaypro 2x
 *
 *	Juergen Buchmueller, July 1998
 *	Benjamin C. W. Sittler, July 1998 (new keyboard table)
 *
 ******************************************************************************/

#include "driver.h"
#include "mess/machine/kaypro.h"
#include "mess/vidhrdw/kaypro.h"
#include "cpu/z80/z80.h"

static const char *disk_ids[4] = {"KAY2","KAY2","KAY2","KAY2"};

static UINT8 keyrows[10] = { 0,0,0,0,0,0,0,0,0,0 };
static char keyboard[8][10][8] = {
  {	/* normal */
    { 27,'1','2','3','4','5','6','7'},
    {'8','9','0','-','=','`',  8,  9},
    {'q','w','e','r','t','y','u','i'},
    {'o','p','[',']', 13,127,  0,  0},
    {'a','s','d','f','g','h','j','k'},
    {'l',';', 39, 92,  0,'z','x','c'},
    {'v','b','n','m',',','.','/',  0},
    { 10,' ','-',',', 13,'.','0','1'},
    {'2','3','4','5','6','7','8','9'},
    { 11, 13,  8, 12,  0,  0,  0,  0},
  },
  {	/* Shift */
    { 27,'!','@','#','$','%','^','&'},
    {'*','(',')','_','+','~',  8,  9},
    {'Q','W','E','R','T','Y','U','I'},
    {'O','P','{','}', 13,127,  0,  0},
    {'A','S','D','F','G','H','J','K'},
    {'L',':','"','|',  0,'Z','X','C'},
    {'V','B','N','M','<','>','?',  0},
    { 10,' ','-',',', 13,'.','0','1'},
    {'2','3','4','5','6','7','8','9'},
    { 11, 13,  8, 12,  0,  0,  0,  0},
  },
  {	/* Control */
    { 27,'1','2','3','4','5','6','7'},
    {'8','9','0','-','=','`',  8,  9},
    { 17, 23,  5, 18, 20, 25, 21,  9},
    { 15, 16, 27, 29, 13,127,  0,  0},
    {  1, 19,  4,  6,  7,  8, 10, 11},
    { 12,';', 39, 28,  0, 26, 24,  3},
    { 22,  2, 14, 13,',','.','/',  0},
    { 10,' ','-',',', 13,'.','0','1'},
    {'2','3','4','5','6','7','8','9'},
    { 11, 13,  8, 12,  0,  0,  0,  0},
  },
  {	/* Shift+Control */
    { 27,'!',  0,'#','$','%', 30,'&'},
    {'*','(',')', 31,'+','~',  8,  9},
    { 17, 23,  5, 18, 20, 25, 21,  9},
    { 15, 16, 27, 29, 13,127,  0,  0},
    {  1, 19,  4,  6,  7,  8, 10, 11},
    { 12,':','"', 28,  0, 26, 24,  3},
    { 22,  2, 14, 13,',','.','/',  0},
    { 10,' ','-',',', 13,'.','0','1'},
    {'2','3','4','5','6','7','8','9'},
    { 11, 13,  8, 12,  0,  0,  0,  0},
  },
  {	/* CapsLock */
    { 27,'1','2','3','4','5','6','7'},
    {'8','9','0','-','=','`',  8,  9},
    {'Q','W','E','R','T','Y','U','I'},
    {'O','P','[',']', 13,127,  0,  0},
    {'A','S','D','F','G','H','J','K'},
    {'L',';', 39, 92,  0,'Z','X','C'},
    {'V','B','N','M',',','.','/',  0},
    { 10,' ','-',',', 13,'.','0','1'},
    {'2','3','4','5','6','7','8','9'},
    { 11, 13,  8, 12,  0,  0,  0,  0},
  },
  {	/* Shift+CapsLock */
    { 27,'!','@','#','$','%','^','&'},
    {'*','(',')','_','+','~',  8,  9},
    {'Q','W','E','R','T','Y','U','I'},
    {'O','P','{','}', 13,127,  0,  0},
    {'A','S','D','F','G','H','J','K'},
    {'L',':','"','|',  0,'Z','X','C'},
    {'V','B','N','M','<','>','?',  0},
    { 10,' ','-',',', 13,'.','0','1'},
    {'2','3','4','5','6','7','8','9'},
    { 11, 13,  8, 12,  0,  0,  0,  0},
  },
  {	/* Control+CapsLock */
    { 27,'1','2','3','4','5','6','7'},
    {'8','9','0','-','=','`',  8,  9},
    { 17, 23,  5, 18, 20, 25, 21,  9},
    { 15, 16, 27, 29, 13,127,  0,  0},
    {  1, 19,  4,  6,  7,  8, 10, 11},
    { 12,';', 39, 28,  0, 26, 24,  3},
    { 22,  2, 14, 13,',','.','/',  0},
    { 10,' ','-',',', 13,'.','0','1'},
    {'2','3','4','5','6','7','8','9'},
    { 11, 13,  8, 12,  0,  0,  0,  0},
  },
  {	/* Shift+Control+CapsLock */
    { 27,'!',  0,'#','$','%', 30,'&'},
    {'*','(',')', 31,'+','~',  8,  9},
    { 17, 23,  5, 18, 20, 25, 21,  9},
    { 15, 16, 27, 29, 13,127,  0,  0},
    {  1, 19,  4,  6,  7,  8, 10, 11},
    { 12,':','"', 28,  0, 26, 24,  3},
    { 22,  2, 14, 13,',','.','/',  0},
    { 10,' ','-',',', 13,'.','0','1'},
    {'2','3','4','5','6','7','8','9'},
    { 11, 13,  8, 12,  0,  0,  0,  0},
  },
};

void kaypro_init_machine(void)
{
  /* disable CapsLock LED initially */
  osd_led_w(1, 1);
  osd_led_w(1, 0);
  cpm_init(4, disk_ids);
}

void kaypro_stop_machine(void)
{
	cpm_exit();
}

int kaypro_rom_load(void)
{
	return 0;
}

int kaypro_rom_id(const char * name, const char * gamename)
{
	return 0;
}

/******************************************************
 * vertical blank interrupt
 * used to scan keyboard; newly pressed keys
 * are stuffed into a keyboard buffer;
 * also drives keyboard LEDs and
 * and handles autorepeating keys
 ******************************************************/
int kaypro_interrupt(void)
{
	int mod, row, col, chg, new;
	static int lastrow = 0, mask = 0x00, key = 0x00, repeat = 0x00, repeater = 0x00;

	if( setup_active() || onscrd_active() )
		return ignore_interrupt();

    if (repeat)
	{
		if ((! -- repeat)) repeater = 4;
	}
	else
	if (repeater)
	{
		repeat = repeater;
	}

	row = 9;
	new = input_port_10_r(0);
	chg = keyrows[row] ^ new;

	if (!chg) { new = input_port_9_r(0); chg = keyrows[--row] ^ new; }
	if (!chg) { new = input_port_8_r(0); chg = keyrows[--row] ^ new; }
	if (!chg) { new = input_port_7_r(0); chg = keyrows[--row] ^ new; }
	if (!chg) { new = input_port_6_r(0); chg = keyrows[--row] ^ new; }
	if (!chg) { new = input_port_5_r(0); chg = keyrows[--row] ^ new; }
	if (!chg) { new = input_port_4_r(0); chg = keyrows[--row] ^ new; }
	if (!chg) { new = input_port_3_r(0); chg = keyrows[--row] ^ new; }
	if (!chg) { new = input_port_2_r(0); chg = keyrows[--row] ^ new; }
	if (!chg) { new = input_port_1_r(0); chg = keyrows[--row] ^ new; }
	if (!chg) --row;

	if (row >= 0)
	{
		repeater = 0x00;
		mask = 0x00;
		key = 0x00;
		lastrow = row;
		/* CapsLock LED */
		if( row == 3 && chg == 0x80 )
			osd_led_w(1, (keyrows[3] & 0x80) ? 0 : 1);

		if (new & chg)	/* key(s) pressed ? */
		{
			mod = 0;

			/* Shift modifier */
			if ( (keyrows[5] & 0x10) || (keyrows[6] & 0x80) )
				mod |= 1;

			/* Control modifier */
			if (keyrows[3] & 0x40)
				mod |= 2;

			/* CapsLock modifier */
			if (keyrows[3] & 0x80)
				mod |= 4;

			/* find new key */
			mask = 0x01;
			for (col = 0; col < 8; col ++)
			{
				if (chg & mask)
				{
					new &= mask;
					key = keyboard[mod][row][col];
					break;
				}
				mask <<= 1;
			}
			if( key )	/* normal key */
			{
				repeater = 30;
				kaypro_conin_w(0, key);
			}
			else
			if( (row == 0) && (chg == 0x04) ) /* Ctrl-@ (NUL) */
				kaypro_conin_w(0, 0);
			keyrows[row] |= new;
		}
		else
		{
			keyrows[row] = new;
		}
		repeat = repeater;
	}
	else
	if ( key && (keyrows[lastrow] & mask) && ! repeat )
	{
		kaypro_conin_w(0, key);
	}
	return ignore_interrupt();
}
