/* Machine functions for the a2600 */

#include "driver.h"


/* local */
unsigned char *a2600_cartridge_rom;

int a2600_TIA_r(int offset)
{
	if (errorlog) fprintf(errorlog,"***A2600 - TIA_r with offset %x\n", offset );
	switch (offset) {
	        case 0x08:
	            if (input_port_1_r(0) & 0x02)
	                return 0x80;
	            else
	                return 0x00;
	        case 0x09:
	            if (input_port_1_r(0) & 0x08)
	                return 0x80;
	            else
	                return 0x00;
	        case 0x0A:
	            if (input_port_1_r(0) & 0x01)
	                return 0x80;
	            else
	                return 0x00;
	        case 0x0B:
	            if (input_port_1_r(0) & 0x04)
	                return 0x80;
	            else
	                return 0x00;
	        case 0x0c:
	            if ((input_port_1_r(0) & 0x08) || (input_port_1_r(0) & 0x02))
	                return 0x00;
	            else
	                return 0x80;
	        case 0x0d:
	            if ((input_port_1_r(0) & 0x01) || (input_port_1_r(0) & 0x04))
	                return 0x00;
	            else
	                return 0x80;
	        default:
	            if (errorlog) fprintf(errorlog,"undefined TIA read %x\n",offset);

	    }
    return 0xFF;


}


void a2600_TIA_w(int offset, int data)
{
	if (errorlog) fprintf(errorlog,"***A2600 - TIA_w with offset %x & data %x\n", offset, data );

}

void a2600_init_machine(void)
{


}


void a2600_stop_machine(void)
{

}


int a2600_id_rom (const char *name, const char *gamename)
{
	return 0;

}




int a2600_load_rom (void)
{
    FILE *cartfile;

	/* A cartridge isn't strictly mandatory, but it's recommended */
	cartfile = NULL;
	if (strlen(rom_name[0])==0)
    {
        if (errorlog) fprintf(errorlog,"A2600 - warning: no cartridge specified!\n");
	}
	else if (!(cartfile = osd_fopen (Machine->gamedrv->name, rom_name[0], OSD_FILETYPE_IMAGE_R, 0)))
	{
        if (errorlog) fprintf(errorlog,"A2600 - Unable to locate cartridge: %s\n",rom_name[0]);
		return 1;
	}
    if (errorlog) fprintf(errorlog,"A2600 - loading Cart - %s \n",rom_name[0]);

    a2600_cartridge_rom = &(ROM[0x1000]);

    if (cartfile!=NULL)
    {
        osd_fread (cartfile, a2600_cartridge_rom, 2048); /* testing COmbat */
        osd_fclose (cartfile);
    }
    else
    {
        return 1;
    }

	return 0;
}


