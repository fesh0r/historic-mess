
#ifndef AUDIT_H
#define AUDIT_H

/* return values from VerifyRomSet and VerifySampleSet */
#define CORRECT   		0
#define NOTFOUND  		1
#define INCORRECT 		2
#define CLONE_NOTFOUND	3

/* rom status values for tAuditRecord.status */
#define AUD_ROM_GOOD		0
#define AUD_ROM_NOT_FOUND	1
#define AUD_BAD_CHECKSUM	2
#define AUD_MEM_ERROR		3
#define AUD_LENGTH_MISMATCH	4

#define AUD_MAX_ROMS		100	/* maximum roms per driver */
#define AUD_MAX_SAMPLES		200	/* maximum samples per driver */

typedef struct
{
	char rom[20];				/* name of rom file */
	unsigned int explength;		/* expected length of rom file */
	unsigned int length;		/* actual length of rom file */
	unsigned int expchecksum;	/* expected checksum of rom file */
	unsigned int checksum;		/* actual checksum of rom file */
	int status;					/* status of rom file */
} tAuditRecord;

typedef struct
{
	char	name[20];		/* name of missing sample file */
} tMissingSample;

typedef void (CLIB_DECL *verify_printf_proc)(char *fmt,...);

int AuditRomSet (int game, tAuditRecord **audit);
int VerifyRomSet(int game,verify_printf_proc verify_printf);
#if 0
int AuditSampleSet (int game, tMissingSample **audit);
int VerifySampleSet(int game,verify_printf_proc verify_printf);
#endif
int RomInSet (const struct GameDriver *gamedrv, unsigned int crc);
int RomsetMissing (int game);


#endif
