/*
This file is a set of function calls and defs required for MESS.
It doesnt do much at the moment, but its here in case anyone
needs it ;-)
*/

#include "driver.h"
#include "mame.h"
#include "mess/mess.h"


char rom_name[MAX_ROM][MAX_PATH]; /* MESS */
char floppy_name[MAX_FLOPPY][MAX_PATH]; /* MESS */
char hard_name[MAX_HARD][MAX_PATH]; /* MESS */
char cassette_name[MAX_CASSETTE][MAX_PATH]; /* MESS */


extern struct GameOptions options;
static int num_roms = 0;
static int num_floppies = 0;
static int num_harddisks = 0;

static char *mess_alpha = "";



/* fileio.c */
void get_alias(char *driver_name, char *argv, char *alias);

void showmessdisclaimer(void)
{
	printf("MESS is an emulator: it reproduces, more or less faithfully, the behaviour of\n"
		 "several computer and console systems. But hardware is useless without software\n"
		 "so an image of the ROMs, cartridges, discs, and cassettes which run on that\n"
		 "hardware is required. Such images, like any other commercial software, are\n"
		 "copyrighted material and it is therefore illegal to use them if you don't own\n"
		 "the original media from which the images are derived. Needless to say, these\n"
		 "images are not distributed together with MESS. Distribution of MESS together\n"
		 "with these images is a violation of copyright law and should be promptly\n"
		 "reported to the authors so that appropriate legal action can be taken.\n\n");
}

void showmessinfo(void)
{
	printf("M.E.S.S. v%s %s\n"
	       "Multiple Emulation Super System - Copyright (C) 1997-98 by the MESS Team\n"
		   "M.E.S.S. is based on the excellent M.A.M.E. Source code\n"
		   "Copyright (C) 1997-99 by Nicola Salmoria and the MAME Team\n\n",build_version,
                                                                        mess_alpha);
		showmessdisclaimer();
		printf("Usage:  MESS machine [image] [options]\n\n"
				"        MESS -list      for a brief list of supported systems\n"
				"        MESS -listfull  for a full list of supported systems\n"
				"See mess.txt for a complete list of options.\n");

}


/**********************************************************/
/* Functions called from MSDOS.C by MAME for running MESS */
/**********************************************************/

int parse_image_types(char *arg)
{
	/* Is it a floppy or a rom? */
	char *ext = strrchr(arg, '.');
	if (ext && (!stricmp(ext,".DSK") || !stricmp(ext,".ATR") || !stricmp(ext,".XFD"))) {
		if (num_floppies >= MAX_FLOPPY) {
			printf("Too many floppy names specified!\n");
			return 1;
		}
		strcpy(options.floppy_name[num_floppies++], arg);
		if (errorlog) fprintf(errorlog,"Using floppy image #%d %s\n", num_floppies, arg);
		return 0;
	}
	if (ext && !stricmp(ext,".IMG")) {
		if (num_harddisks >= MAX_HARD) {
			printf("Too many hard disk image names specified!\n");
			return 1;
		}
		strcpy(options.hard_name[num_harddisks++], arg);
		if (errorlog) fprintf(errorlog,"Using hard disk image #%d %s\n", num_harddisks, arg);
		return 0;
	}
	if (num_roms >= MAX_ROM) {
		printf("Too many ROM image names specified!\n");
		return 1;
	}
	strcpy(options.rom_name[num_roms++], arg);
	if (errorlog) fprintf(errorlog,"Using ROM image #%d %s\n", num_roms, arg);
	return 0;
}









int load_image(int argc, char **argv, char *driver, int j)
{

	/*
	* Take all additional commandline arguments without "-" as image
	* names. This is an ugly hack that will hopefully eventually be
	* replaced with an online version that lets you "hot-swap" images.
	* HJB 08/13/98 for now the hack is extended even more :-/
	* Skip arguments to options starting with "-" too and accept
	* aliases for a set of ROMs/images.
    */
    int i;
    int res=0;
	char * alias;



	for (i = j+1; i < argc; i++)
    {

		alias=malloc(sizeof(char));

        /* skip options and their additional arguments */
		if (argv[i][0] == '-')
        {
			/* removed */
        }
        else
        {
			/* check if this is an alias for a set of images */

			//alias = get_config_string(driver, argv[i], "");
			get_alias(driver,argv[i],alias);
			if (alias && strlen(alias))
            {
				char *arg;
				if (errorlog)
					fprintf(errorlog,"Using alias %s (%s) for driver %s\n", argv[i],alias, driver);
				arg = strtok (alias, ",");
				while (arg)
            	{
					res = parse_image_types(arg);
					arg = strtok(0, ",");
				}
			}
            else
            {
			if (errorlog)
				fprintf(errorlog,"NOTE: No alias found\n");
				res = parse_image_types(argv[i]);
			}
			/* free up the alias - dont need it anymore */
			free(alias);
		}
		/* If we had an error leave now */
		if (res)
			return res;
		}
    return res;
}
