/**********************************************************************

	Rockwell 6522 VIA interface and emulation

	This function emulates all the functionality of up to 8 6522
	versatile interface adapters.

    This is based on the M6821 emulation in MAME.

    Only the Vectrex driver uses this so far. Emulation is very
    incomplete and covers just things needed by the Vectrex.

**********************************************************************/

#ifndef VIA_6522
#define VIA_6522


#define MAX_VIA 8


/* this is the standard ordering of the registers */
/* alternate ordering swaps registers 1 and 2 */
#define	VIA_PB	    0
#define	VIA_PA	    1
#define	VIA_DDRB    2
#define	VIA_DDRA    3
#define	VIA_T1CL    4
#define	VIA_T1CH    5
#define	VIA_T1LL    6
#define	VIA_T1LH    7
#define	VIA_T2CL    8
#define	VIA_T2CH    9
#define	VIA_SR     10
#define	VIA_ACR    11
#define	VIA_PCR    12
#define	VIA_IFR    13
#define	VIA_IER    14
#define	VIA_PANH   15

struct via6522_interface
{
	int (*in_a_func)(int offset);
	int (*in_b_func)(int offset);
	int (*in_ca1_func)(int offset);
	int (*in_cb1_func)(int offset);
	int (*in_ca2_func)(int offset);
	int (*in_cb2_func)(int offset);
	void (*out_a_func)(int offset, int val);
	void (*out_b_func)(int offset, int val);
	void (*out_ca2_func)(int offset, int val);
	void (*out_cb2_func)(int offset, int val);
	void (*irq_func)(int state);

    /* kludges for the Vectrex */
	void (*out_shift_func)(int val);
	void (*t2_callback)(double time);
};


void via2_config(int which, const struct via6522_interface *intf);
void via2_reset(void);
int via2_read(int which, int offset);
void via2_write(int which, int offset, int data);
void via2_set_input_a(int which, int data);
void via2_set_input_ca1(int which, int data);
void via2_set_input_ca2(int which, int data);
void via2_set_input_b(int which, int data);
void via2_set_input_cb1(int which, int data);
void via2_set_input_cb2(int which, int data);

/******************* Standard 8-bit CPU interfaces, D0-D7 *******************/

int via2_0_r(int offset);
int via2_1_r(int offset);
int via2_2_r(int offset);
int via2_3_r(int offset);
int via2_4_r(int offset);
int via2_5_r(int offset);
int via2_6_r(int offset);
int via2_7_r(int offset);

void via2_0_w(int offset, int data);
void via2_1_w(int offset, int data);
void via2_2_w(int offset, int data);
void via2_3_w(int offset, int data);
void via2_4_w(int offset, int data);
void via2_5_w(int offset, int data);
void via2_6_w(int offset, int data);
void via2_7_w(int offset, int data);

/******************* 8-bit A/B port interfaces *******************/

void via2_0_porta_w(int offset, int data);
void via2_1_porta_w(int offset, int data);
void via2_2_porta_w(int offset, int data);
void via2_3_porta_w(int offset, int data);
void via2_4_porta_w(int offset, int data);
void via2_5_porta_w(int offset, int data);
void via2_6_porta_w(int offset, int data);
void via2_7_porta_w(int offset, int data);

void via2_0_portb_w(int offset, int data);
void via2_1_portb_w(int offset, int data);
void via2_2_portb_w(int offset, int data);
void via2_3_portb_w(int offset, int data);
void via2_4_portb_w(int offset, int data);
void via2_5_portb_w(int offset, int data);
void via2_6_portb_w(int offset, int data);
void via2_7_portb_w(int offset, int data);

int via2_0_porta_r(int offset);
int via2_1_porta_r(int offset);
int via2_2_porta_r(int offset);
int via2_3_porta_r(int offset);
int via2_4_porta_r(int offset);
int via2_5_porta_r(int offset);
int via2_6_porta_r(int offset);
int via2_7_porta_r(int offset);

int via2_0_portb_r(int offset);
int via2_1_portb_r(int offset);
int via2_2_portb_r(int offset);
int via2_3_portb_r(int offset);
int via2_4_portb_r(int offset);
int via2_5_portb_r(int offset);
int via2_6_portb_r(int offset);
int via2_7_portb_r(int offset);

/******************* 1-bit CA1/CA2/CB1/CB2 port interfaces *******************/

void via2_0_ca1_w(int offset, int data);
void via2_1_ca1_w(int offset, int data);
void via2_2_ca1_w(int offset, int data);
void via2_3_ca1_w(int offset, int data);
void via2_4_ca1_w(int offset, int data);
void via2_5_ca1_w(int offset, int data);
void via2_6_ca1_w(int offset, int data);
void via2_7_ca1_w(int offset, int data);
void via2_0_ca2_w(int offset, int data);
void via2_1_ca2_w(int offset, int data);
void via2_2_ca2_w(int offset, int data);
void via2_3_ca2_w(int offset, int data);
void via2_4_ca2_w(int offset, int data);
void via2_5_ca2_w(int offset, int data);
void via2_6_ca2_w(int offset, int data);
void via2_7_ca2_w(int offset, int data);

void via2_0_cb1_w(int offset, int data);
void via2_1_cb1_w(int offset, int data);
void via2_2_cb1_w(int offset, int data);
void via2_3_cb1_w(int offset, int data);
void via2_4_cb1_w(int offset, int data);
void via2_5_cb1_w(int offset, int data);
void via2_6_cb1_w(int offset, int data);
void via2_7_cb1_w(int offset, int data);
void via2_0_cb2_w(int offset, int data);
void via2_1_cb2_w(int offset, int data);
void via2_2_cb2_w(int offset, int data);
void via2_3_cb2_w(int offset, int data);
void via2_4_cb2_w(int offset, int data);
void via2_5_cb2_w(int offset, int data);
void via2_6_cb2_w(int offset, int data);
void via2_7_cb2_w(int offset, int data);

int via2_0_ca1_r(int offset);
int via2_1_ca1_r(int offset);
int via2_2_ca1_r(int offset);
int via2_3_ca1_r(int offset);
int via2_4_ca1_r(int offset);
int via2_5_ca1_r(int offset);
int via2_6_ca1_r(int offset);
int via2_7_ca1_r(int offset);
int via2_0_ca2_r(int offset);
int via2_1_ca2_r(int offset);
int via2_2_ca2_r(int offset);
int via2_3_ca2_r(int offset);
int via2_4_ca2_r(int offset);
int via2_5_ca2_r(int offset);
int via2_6_ca2_r(int offset);
int via2_7_ca2_r(int offset);

int via2_0_cb1_r(int offset);
int via2_1_cb1_r(int offset);
int via2_2_cb1_r(int offset);
int via2_3_cb1_r(int offset);
int via2_4_cb1_r(int offset);
int via2_5_cb1_r(int offset);
int via2_6_cb1_r(int offset);
int via2_7_cb1_r(int offset);
int via2_0_cb2_r(int offset);
int via2_1_cb2_r(int offset);
int via2_2_cb2_r(int offset);
int via2_3_cb2_r(int offset);
int via2_4_cb2_r(int offset);
int via2_5_cb2_r(int offset);
int via2_6_cb2_r(int offset);
int via2_7_cb2_r(int offset);


#endif
