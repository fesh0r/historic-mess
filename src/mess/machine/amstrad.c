/***************************************************************************

  machine.c

  Functions to emulate general aspects of the machine (RAM, ROM, interrupts,
  I/O ports)

Amstrad hardware consists of:

- General Instruments AY-3-8912 (audio and keyboard scanning)
- Intel 8255PPI (keyboard, access to AY-3-8912, cassette etc)
- Z80A CPU
- 765 FDC (disc drive interface)
- "Gate Array" (custom chip by Amstrad controlling colour, mode,
rom/ram selection


***************************************************************************/

#include <stdarg.h>
#include "driver.h"
#include "cpu/z80/z80.h"
#include "mess/vidhrdw/m6845.h"
#include "mess/machine/amstrad.h"
#include "mess/systems/i8255.h"

void    AmstradCPC_GA_Write(int);
void    AmstradCPC_SetUpperRom(int);
void    Amstrad_RethinkMemory(void);
void    Amstrad_Init(void);
void    amstrad_sh_control_port_w(int);
void    amstrad_sh_data_port_w(int);
void    amstrad_handle_snapshot(unsigned char *);


/* hd6845s functions */
int     hd6845s_index_r(void);
int     hd6845s_register_r(void);
void    hd6845s_index_w(int data);
void    hd6845s_register_w(int data);
int 	hd6845s_getreg(int);


static unsigned char *file_loaded = NULL;
static int snapshot_loaded = 0;

extern unsigned char *Amstrad_Memory;

int amstrad_opbaseoverride(int pc)
{
  /* clear op base override */
  cpu_setOPbaseoverride(0);

  if (snapshot_loaded)
  {
        /* its a snapshot file - setup hardware state */
        amstrad_handle_snapshot(file_loaded);
  }

  return (cpu_get_pc() & 0x0ffff);
}

void    amstrad_init_machine(void)
{
        /* allocate ram - I control how it is accessed so I must
        allocate it somewhere - here will do */
        Amstrad_Memory = malloc(128*1024);

        Amstrad_Init();

        if (file_loaded!=NULL)
        {
                if (memcmp(&file_loaded[0],"MV - SNA",8)==0)
                {
                   snapshot_loaded = 1;

                   cpu_setOPbaseoverride(amstrad_opbaseoverride);
                }
        }

        if (!snapshot_loaded)
        {
                Amstrad_Reset();
        }
}

void    amstrad_shutdown_machine(void)
{
	if (Amstrad_Memory != NULL)
		free(Amstrad_Memory);

	Amstrad_Memory = NULL;

        if (file_loaded!=NULL)
                free(file_loaded);


}

/* load CPCEMU style snapshots */
void    amstrad_handle_snapshot(unsigned char *pSnapshot)
{
        int RegData;
        int i;


        /* init Z80 */
        RegData = (pSnapshot[0x011] & 0x0ff) | ((pSnapshot[0x012] & 0x0ff)<<8);
        cpu_set_reg(Z80_AF, RegData);

        RegData = (pSnapshot[0x013] & 0x0ff) | ((pSnapshot[0x014] & 0x0ff)<<8);
        cpu_set_reg(Z80_BC, RegData);

        RegData = (pSnapshot[0x015] & 0x0ff) | ((pSnapshot[0x016] & 0x0ff)<<8);
        cpu_set_reg(Z80_DE, RegData);

        RegData = (pSnapshot[0x017] & 0x0ff) | ((pSnapshot[0x018] & 0x0ff)<<8);
        cpu_set_reg(Z80_HL, RegData);

        RegData = (pSnapshot[0x019] & 0x0ff) ;
        cpu_set_reg(Z80_R, RegData);

        RegData = (pSnapshot[0x01a] & 0x0ff);
        cpu_set_reg(Z80_I, RegData);

        if ((pSnapshot[0x01b] & 1)==1)
        {
                cpu_set_reg(Z80_IFF1, 1);
        }
        else
        {
                cpu_set_reg(Z80_IFF1, 0);
        }

        if ((pSnapshot[0x01c] & 1)==1)
        {
                cpu_set_reg(Z80_IFF2, 1);
        }
        else
        {
                cpu_set_reg(Z80_IFF2, 0);
        }

        RegData = (pSnapshot[0x01d] & 0x0ff) | ((pSnapshot[0x01e] & 0x0ff)<<8);
        cpu_set_reg(Z80_IX, RegData);

        RegData = (pSnapshot[0x01f] & 0x0ff) | ((pSnapshot[0x020] & 0x0ff)<<8);
        cpu_set_reg(Z80_IY, RegData);

        RegData = (pSnapshot[0x021] & 0x0ff) | ((pSnapshot[0x022] & 0x0ff)<<8);
        cpu_set_reg(Z80_SP, RegData);
       cpu_set_sp(RegData);

        RegData = (pSnapshot[0x023] & 0x0ff) | ((pSnapshot[0x024] & 0x0ff)<<8);

        cpu_set_reg(Z80_PC, RegData);
        cpu_set_pc(RegData);

        RegData = (pSnapshot[0x025] & 0x0ff);
        cpu_set_reg(Z80_IM, RegData);

        RegData = (pSnapshot[0x026] & 0x0ff) | ((pSnapshot[0x027] & 0x0ff)<<8);
        cpu_set_reg(Z80_AF2, RegData);

        RegData = (pSnapshot[0x028] & 0x0ff) | ((pSnapshot[0x029] & 0x0ff)<<8);
        cpu_set_reg(Z80_BC2, RegData);

        RegData = (pSnapshot[0x02a] & 0x0ff) | ((pSnapshot[0x02b] & 0x0ff)<<8);
        cpu_set_reg(Z80_DE2, RegData);

        RegData = (pSnapshot[0x02c] & 0x0ff) | ((pSnapshot[0x02d] & 0x0ff)<<8);
        cpu_set_reg(Z80_HL2, RegData);



        /* init GA */
        for (i=0; i<17; i++)
        {
                AmstradCPC_GA_Write(i);

                AmstradCPC_GA_Write(((pSnapshot[0x02f + i] & 0x01f) | 0x040));
        }

        AmstradCPC_GA_Write(pSnapshot[0x02e] & 0x01f);

        AmstradCPC_GA_Write(((pSnapshot[0x040] & 0x03f) | 0x080));

        AmstradCPC_GA_Write(((pSnapshot[0x041] & 0x03f) | 0x0c0));

        /* init CRTC */
        for (i=0; i<18; i++)
        {
                hd6845s_index_w(i);
                hd6845s_register_w(pSnapshot[0x043+i] & 0x0ff);
        }

        hd6845s_index_w(pSnapshot[0x042] & 0x0ff);

        /* upper rom selection */
        AmstradCPC_SetUpperRom(pSnapshot[0x055]);

        /* PPI */
        i8255_control_port_w(pSnapshot[0x059] & 0x0ff);

        i8255_porta_w(pSnapshot[0x056] & 0x0ff);
        i8255_portb_w(pSnapshot[0x057] & 0x0ff);
        i8255_portc_w(pSnapshot[0x058] & 0x0ff);

        /* PSG */
        for (i=0; i<16; i++)
        {
                amstrad_sh_control_port_w(i);

                amstrad_sh_data_port_w(pSnapshot[0x05b + i] & 0x0ff);
        }

        amstrad_sh_control_port_w(pSnapshot[0x05a]);

        {
                int MemSize;
                int MemorySize;

                MemSize = (pSnapshot[0x06b] & 0x0ff) | ((pSnapshot[0x06c] & 0x0ff)<<8);
                
                if (MemSize==128)
                {
                        MemorySize = 128*1024;
                }
                else
                {
                        MemorySize = 64*1024;
                }

                memcpy(Amstrad_Memory, &pSnapshot[0x0100], MemorySize);

        }

        Amstrad_RethinkMemory();

}

int	amstrad_rom_load()
{
        void *file;
                
        file = osd_fopen(Machine->gamedrv->name, rom_name[0], OSD_FILETYPE_ROM_CART, 0);

        if (file)
        {
                int datasize;
                unsigned char *data;

                /* get file size */
                datasize = osd_fsize(file);

                if (datasize!=0)
                {
                        /* malloc memory for this data */
                        data = malloc(datasize);
        
                        if (data!=NULL)
                        {
                                /* read whole file */
                                osd_fread(file, data, datasize);
        
                                file_loaded = data;
        
                                /* close file */
                                osd_fclose(file);

                                /* ok! */
                                return 0;
                        }
                        osd_fclose(file);

                }


                return 1;
        }

        return 0;
}

int	amstrad_rom_id(const char *name, const char *gamename)
{
	return 1;
}

