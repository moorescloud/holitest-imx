/*
 * iMX233 Holiday Test Code
 *
 */


#include <debug.h>
#include "regsuartdbg.h"
#include "regsuartapp.h"
#include "regsclkctrl.h"
#include "regspinctrl.h"
#include "regsdigctl.h"
#include "regspower.h"
#include "regsocotp.h"
#include "regsssp_i.h"
#include "regsi2c.h"
#include <stdarg.h>

unsigned int rom0 = 0;
unsigned int cust0 = 0;
unsigned int cust1 = 0;
unsigned int cust2 = 0;
unsigned int cust3 = 0;
unsigned int lock = 0;

 /* Debug uart have been init by boot rom. */
void putc(char ch)
{
	int loop = 0;
	while (HW_UARTDBGFR_RD()&BM_UARTDBGFR_TXFF)
		if (++loop > 10000) break;

	/* if(!(HW_UARTDBGFR_RD() &BM_UARTDBGFR_TXFF)) */
	HW_UARTDBGDR_WR(ch);
}

int getc()
{
	if (HW_UARTDBGFR_RD()&BM_UARTDBGFR_RXFE)
		return -1;
	return (HW_UARTDBGDR_RD()&BM_UARTDBGDR_DATA);	
}

void pinit()
{
	//printf("HW_PINCTRL_MUXSEL3=0x%x\r\n", HW_PINCTRL_MUXSEL3_RD());

	// enable debug uart receiver
	HW_UARTDBGCR_SET(BM_UARTDBGCR_RXE);

	// select DUART functions on gpio pins
	HW_PINCTRL_MUXSEL3_SET(0x00F00000);
	HW_PINCTRL_MUXSEL3_CLR(0x00500000);
}

#define UARTCLK		24000000
#define BAUD_RATE	115200

/* configure application uart - connects to ATmega */
void ainit()
{
	unsigned int start;

	int divisor = UARTCLK*32 / BAUD_RATE;

	//reset application uart - just in case
	HW_UARTAPP_CTRL0_CLR(1, BM_UARTAPP_CTRL0_SFTRST);

	// wait for reset, but always at least 2us
        start = HW_DIGCTL_MICROSECONDS_RD();
        while ((HW_UARTAPP_CTRL0_RD(1)&BM_UARTAPP_CTRL0_SFTRST) |
		(HW_DIGCTL_MICROSECONDS_RD() < start+2)) ;

	HW_UARTAPP_CTRL0_CLR(1, BM_UARTAPP_CTRL0_CLKGATE);

	HW_UARTAPP_CTRL0_SET(1, BM_UARTAPP_CTRL0_SFTRST);

        while (!(HW_UARTAPP_CTRL0_RD(1)&BM_UARTAPP_CTRL0_CLKGATE)) ;

	HW_UARTAPP_CTRL0_CLR(1, BM_UARTAPP_CTRL0_SFTRST);

	// wait for reset, but always at least 2us
        start = HW_DIGCTL_MICROSECONDS_RD();
        while ((HW_UARTAPP_CTRL0_RD(1)&BM_UARTAPP_CTRL0_SFTRST) |
		(HW_DIGCTL_MICROSECONDS_RD() < start+2)) ;

	HW_UARTAPP_CTRL0_CLR(1, BM_UARTAPP_CTRL0_CLKGATE);

        while (HW_UARTAPP_CTRL0_RD(1)&BM_UARTAPP_CTRL0_CLKGATE)
		;

	// set up baud rate divisor
	HW_UARTAPP_LINECTRL_CLR(1, BM_UARTAPP_LINECTRL_BAUD_DIVINT|BM_UARTAPP_LINECTRL_BAUD_DIVFRAC);
	HW_UARTAPP_LINECTRL_SET(1, ((divisor>>6)<<BP_UARTAPP_LINECTRL_BAUD_DIVINT)|((divisor&0x3f)<<BP_UARTAPP_LINECTRL_BAUD_DIVFRAC));
	HW_UARTAPP_LINECTRL_SET(1, BM_UARTAPP_LINECTRL_WLEN); /*8 bits*/
	HW_UARTAPP_CTRL2_SET(1, BM_UARTAPP_CTRL2_UARTEN);

	//printf("HW_PINCTRL_MUXSEL1=0x%x\r\n", HW_PINCTRL_MUXSEL1_RD());

	// select AUART functions on gpio pins
	HW_PINCTRL_MUXSEL1_SET(0xF0000000);
	HW_PINCTRL_MUXSEL1_CLR(0x50000000);
}

void aputc(char ch)
{
        int loop = 0;
        while (HW_UARTAPP_STAT_RD(1)&BM_UARTAPP_STAT_TXFF)
                if (++loop > 10000) break;

        /* if(!(HW_UARTAPP_STAT_RD(1) &BM_UARTAPP_STAT_TXFF)) */
        HW_UARTAPP_DATA_WR(1, ch);
}

int agetc()
{
	if (HW_UARTAPP_STAT_RD(1)&BM_UARTAPP_STAT_RXFE)
		return -1;
	return (HW_UARTAPP_DATA_RD(1));	
}

void delay(unsigned int us)
{
	unsigned int start , cur;
	start = cur = HW_DIGCTL_MICROSECONDS_RD();

	while (cur < start+us)
		cur = HW_DIGCTL_MICROSECONDS_RD();
}

unsigned int otp_read(unsigned int addr)
{
	unsigned int val;

	// special steps to read non shadowed OTP regs

	// set HW_OCOTP_CTRL_RD_BANK_OPEN
	HW_OCOTP_CTRL_SET(BM_OCOTP_CTRL_RD_BANK_OPEN);
	// poll for HW_OCOTP_CTRL_BUSY to set (takes 33 HCLK cycles)
	while (!(HW_OCOTP_CTRL_RD()&BM_OCOTP_CTRL_BUSY)) ;
	// poll for HW_OCOTP_CTRL_BUSY to clear
	while (HW_OCOTP_CTRL_RD()&BM_OCOTP_CTRL_BUSY) ;

	val = *(volatile reg32_t *)addr;

	// clear HW_OCOTP_CTRL_RD_BANK_OPEN
	HW_OCOTP_CTRL_CLR(BM_OCOTP_CTRL_RD_BANK_OPEN);

	return val;
}

int otp_write(unsigned int addr, unsigned int val)
{
	unsigned int temp;
	int idx = (((addr &0xffff) - 0x20) / 0x10) &BM_OCOTP_CTRL_ADDR;

	//printf("ocotp write addr 0x%x idx 0x%x with val 0x%x\r\n" , addr, idx, val);

	//make sure nothing in wrong state
	if (HW_OCOTP_CTRL_RD()&BM_OCOTP_CTRL_BUSY)
	{
		printf("ocotp ctrl busy\r\n");
		return 0;
	}
	if (HW_OCOTP_CTRL_RD()&BM_OCOTP_CTRL_ERROR)
	{
		printf("ocotp ctrl error\r\n");
		return 0;
	}
	if (HW_OCOTP_CTRL_RD()&BM_OCOTP_CTRL_RD_BANK_OPEN)
	{
		printf("ocotp ctrl rd bank open\r\n");
		return 0;
	}

	// set HCLK to 24MHz (max) for OTP writes
	unsigned int oldhclk = HW_CLKCTRL_HBUS_RD();
	temp = oldhclk;
        temp &= ~BM_CLKCTRL_HBUS_DIV;
        temp |= BF_CLKCTRL_HBUS_DIV(19); /*change to (454/19)=~24MHz*/
        HW_CLKCTRL_HBUS_WR(temp);
	while (HW_CLKCTRL_HBUS_RD()&BM_CLKCTRL_HBUS_BUSY) ;

	//printf("slow hbus 0x%x\r\n" , HW_CLKCTRL_HBUS_RD());

	// set VDDIO to 2.8V
	unsigned int oldvddio = HW_POWER_VDDIOCTRL_RD();
	temp = oldvddio;
        temp &= ~BM_POWER_VDDIOCTRL_TRG;
        temp |= BF_POWER_VDDIOCTRL_TRG(0); /*change to 2.800v*/
        HW_POWER_VDDIOCTRL_WR(temp);
        delay(20000);

	//printf("low vddio 0x%x\r\n" , HW_POWER_VDDIOCTRL_RD());

	HW_OCOTP_CTRL_CLR(BM_OCOTP_CTRL_ADDR|BM_OCOTP_CTRL_WR_UNLOCK);
	HW_OCOTP_CTRL_SET((idx&BM_OCOTP_CTRL_ADDR)<<BP_OCOTP_CTRL_ADDR);
	HW_OCOTP_CTRL_SET(BV_OCOTP_CTRL_WR_UNLOCK__KEY<<BP_OCOTP_CTRL_WR_UNLOCK);
	
	HW_OCOTP_DATA_WR(val);

	// poll for HW_OCOTP_CTRL_BUSY to clear
	while (HW_OCOTP_CTRL_RD()&BM_OCOTP_CTRL_BUSY) ;

	// must wait at least 2us after write OCOTP
	delay(10);

	// restore VDDIO
        HW_POWER_VDDIOCTRL_WR(oldvddio);
        delay(20000);

	// restore HCLK
        HW_CLKCTRL_HBUS_WR(oldhclk);
	while (HW_CLKCTRL_HBUS_RD()&BM_CLKCTRL_HBUS_BUSY) ;

	//printf("restored hbus 0x%x\r\n" , HW_CLKCTRL_HBUS_RD());
	//printf("restored vddio 0x%x\r\n" , HW_POWER_VDDIOCTRL_RD());

        if (HW_OCOTP_CTRL_RD()&BM_OCOTP_CTRL_ERROR)
        {
		HW_OCOTP_CTRL_CLR(BM_OCOTP_CTRL_ERROR);
                printf("ocotp ctrl error\r\n");
                return 0;
        }

	return 1;
}

void dump_ocotp()
{
	// force a reload (needed only for LOCK bits?)
	HW_OCOTP_CTRL_SET(BM_OCOTP_CTRL_RELOAD_SHADOWS);

	// poll for HW_OCOTP_CTRL_BUSY and BM_OCOTP_CTRL_RELOAD_SHADOWS to clear
	while (HW_OCOTP_CTRL_RD()&(BM_OCOTP_CTRL_BUSY|BM_OCOTP_CTRL_RELOAD_SHADOWS)) ;

	// ROM0 is shadowed, so no special read required
	printf("ROM0=0x%x ", HW_OCOTP_ROMn_RD(0));

	// read non shadowed OTP regs
	printf("CUST0=0x%x ", otp_read(HW_OCOTP_CUSTn_ADDR(0)));
	printf("CUST1=0x%x ", otp_read(HW_OCOTP_CUSTn_ADDR(1)));
	printf("CUST2=0x%x ", otp_read(HW_OCOTP_CUSTn_ADDR(2)));
	printf("CUST3=0x%x ", otp_read(HW_OCOTP_CUSTn_ADDR(3)));

	// LOCK is shadowed, so no special read required
	printf("LOCK=0x%x\r\n", HW_OCOTP_LOCK_RD());
}

void update_ocotp()
{
	int ch;
	while ((ch = getc())<0) ;
	putc(ch);
	if (ch!='!')
	{
		printf("Abort\r\n");
		return;
	}

	int ret = 1;	// otp_write returns 0 on error
	if (rom0!=0 && ret) ret = otp_write(HW_OCOTP_ROMn_ADDR(0), rom0);
	if (cust0!=0 && ret) ret = otp_write(HW_OCOTP_CUSTn_ADDR(0), cust0);
	if (cust1!=0 && ret) ret = otp_write(HW_OCOTP_CUSTn_ADDR(1), cust1);
	if (cust2!=0 && ret) ret = otp_write(HW_OCOTP_CUSTn_ADDR(2), cust2);
	if (cust3!=0 && ret) ret = otp_write(HW_OCOTP_CUSTn_ADDR(3), cust3);
	if (lock!=0 && ret) ret = otp_write(HW_OCOTP_LOCK_ADDR, lock);

	if (ret) dump_ocotp();
}

int get_hex(unsigned int *p)
{
	unsigned int val = 0;
	int n = 0, ch;
	while (n<8)
	{
		while ((ch = getc())<0) ;
		if (ch>='0'&&ch<='9') { putc(ch); ch -= '0'; }
		else if (ch>='A'&&ch<='F') { putc(ch); ch -= ('A' - 10); }
		else if (ch>='a'&&ch<='f') { putc(ch); ch -= ('a' - 10); }
		else { return 0; }
		val = (val<<4) | ch;
		n++;
	}
	*p = val;
	return 1;
}

#define MEMTESTADDR 0x40000000
#define MEMTESTSIZE (64*1024*1024/sizeof(int))
#define MEMTESTTEXT "64MB"

void mem_test()
{
	volatile int *p;
        int i, e = 0;

        printf("memory test (" MEMTESTTEXT "): ");

        p = (volatile int *)MEMTESTADDR;
        i = 0;
        while (i < MEMTESTSIZE)
	{
                *p++ = i++;
                if ((i&0xfffff)==0) printf(".");
        }

        p = (volatile int *)MEMTESTADDR;
        i = 0;
        while (i < MEMTESTSIZE)
	{
                if (*p != i)
		{
                        printf("\r\n0x%x error value 0x%x", i, *p);
                        e++;
                }
                p++;
                i++;
                if ((i&0xfffff)==0) printf("+");
        }

        p = (volatile int *)MEMTESTADDR;
        i = 0;
        while (i < MEMTESTSIZE)
	{
                *p++ = ~i++;
                if ((i&0xfffff)==0) printf(".");
        }

        p = (volatile int *)MEMTESTADDR;
        i = 0;
        while (i < MEMTESTSIZE)
	{
                if (*p != ~i)
		{
                        printf("\r\n0x%x error value 0x%x", ~i, *p);
                        e++;
                }
                p++;
                i++;
                if ((i&0xfffff)==0) printf("+");
        }

        if (e) printf("\r\n0x%x errors\r\n", e);
        else printf("passed\r\n");

}

int _start(int arg);

void dump_ram()
{
	unsigned int *p = 0;
	int i = 0;

	printf("_start@0x%x\r\n", _start);

	while (i<32768/sizeof(int))
	{
		if ((i>0)&&(i&0x3f)==0)
		{
			int ch;
			while ((ch = getc())<0) ;
			putc(ch);
			if (ch=='-') { i -= 128; p -= 128; }
			else if ((ch!=' ')&&(ch!='+')) break;
		}
		if ((i&3)==0) printf("\r\n0x%x:", i * 4);
		printf(" 0x%x", *p);
		p++;
		i++;
	}
	printf("\r\n");
}

void reset_atmega()
{
	// toggle GPIO16 low then high (BANK0 pin 16)
	// actually just toggle output state, and rely on external pullups
	HW_PINCTRL_MUXSEL1_SET(0x00000003);	// make GPIO
	HW_PINCTRL_DRIVE2_CLR(0x00000001);	// set current
	HW_PINCTRL_DRIVE2_SET(0x00000002);	// set current 12mA
	HW_PINCTRL_DOUT0_CLR(0x00010000);	// set out state low
	HW_PINCTRL_DOE0_SET(0x00010000);	// make output
	delay(2000);
	printf("RESET\r\n");
	HW_PINCTRL_DOE0_CLR(0x00010000);	// make input
}

void i2c_write(char erase)
{
	// basic erase or prep of I2C EEPROM
	// if erase is set, clear first page (32 bytes)
	// else write first 6 bytes with default values
	// should detect I2C failure/timeout

	// note - per comments in Linux source:
//The i.MX23 I2C controller is also capable of PIO, but needs a little harder
//push to behave. The controller needs to be reset after every PIO/DMA operation
//for some reason, otherwise in rare cases, the controller can hang or emit
//bytes onto the bus.

	// select I2C functions on gpio pins
	HW_PINCTRL_MUXSEL3_SET(0x0003C000);
	HW_PINCTRL_MUXSEL3_CLR(0x00028000);

	int i;
	unsigned int data, start;
	//unsigned char eeval[] = { 'h', 0x01, 0x08, 50, 0, 0 };
	//array init requires memcpy... so we hack in a switch statment
	unsigned char eeval;

	for (i=0; i<(erase?32:6); i++)
	{
		HW_I2C_CTRL0_SET(BM_I2C_CTRL0_SFTRST);
		delay(1000);

		HW_I2C_CTRL0_CLR(BM_I2C_CTRL0_CLKGATE);
		delay(1000);

		HW_I2C_CTRL0_CLR(BM_I2C_CTRL0_SFTRST);
		delay(1000);

		// high time = 120 clocks, read bit at 48 for 95KHz at 24MHz
		HW_I2C_TIMING0_WR(0x00780030);
		// low time at 128, write bit at 48 for 95 kHz at 24 MHz
		HW_I2C_TIMING1_WR(0x00800030);

		// note PIO mode only allows writes up to 3 data bytes
		// requires DMA for reads! - so we don't bother for now
		HW_I2C_CTRL0_SET(BM_I2C_CTRL0_PIO_MODE|BM_I2C_CTRL0_MASTER_MODE|BM_I2C_CTRL0_DIRECTION|BM_I2C_CTRL0_PRE_SEND_START|BM_I2C_CTRL0_POST_SEND_STOP);

		eeval = 0xff;
		if (!erase) switch(i)
		{
			case 0: eeval = 'h'; break;
			case 1: eeval = 1; break;
			case 2: eeval = 8; break;
			case 3: eeval = 50; break;
			case 4: eeval = 0; break;
			case 5: eeval = 0; break;
		}

		// write to EEPROM @ 0x50 (0xA0)
		data = eeval<<24 | i<<16 | 0xA0;
		//printf("I2C DATA = 0x%x\r\n", data);
		HW_I2C_DATA_WR(data);

		HW_I2C_CTRL0_CLR(BM_I2C_CTRL0_XFER_COUNT);
		HW_I2C_CTRL0_SET(4);	// write 4 bytes to bus

		//printf("setting I2C RUN\r\n");
		HW_I2C_CTRL0_SET(BM_I2C_CTRL0_RUN);

        	start = HW_DIGCTL_MICROSECONDS_RD();
		//printf("waiting for I2C RUN to go high\r\n");
		while (!(HW_I2C_CTRL0_RD()&BM_I2C_CTRL0_RUN))
			if (HW_DIGCTL_MICROSECONDS_RD() > start+5000) break;
		if (!(HW_I2C_CTRL0_RD()&BM_I2C_CTRL0_RUN))
		{
			printf("I2C_RUN timeout 1\r\n");
			return;
		}

        	start = HW_DIGCTL_MICROSECONDS_RD();
		//printf("waiting for I2C RUN to go low\r\n");
		while (HW_I2C_CTRL0_RD()&BM_I2C_CTRL0_RUN)
			if (HW_DIGCTL_MICROSECONDS_RD() > start+50000) break;
		if (HW_I2C_CTRL0_RD()&BM_I2C_CTRL0_RUN)
		{
			printf("I2C_RUN timeout 2\r\n");
			return;
		}

		// allow eeprom time to save data
		delay(10000);
	}

	if (erase)
		printf("I2C EEPROM erased\r\n");
	else
		printf("I2C EEPROM prepped\r\n");
}

#define SSP2_DIV 10	//must be even from 2 to 254
#define SSP2_RATE 9	//0 to 255, 1 is added

void spi_test()
{
	unsigned int data, start;
	int i;

	if (!get_hex(&data))
	{
		printf("Abort\r\n");
		return;
	}
	printf(":");

	//printf("HW_CLKCTRL_SSP=0x%x\r\n", HW_CLKCTRL_SSP_RD());
	//HW_CLKCTRL_SSP = 0x00000001 : enabled & divider = 1 (bootrom?)

	// select SSP2_SCK & SSP2_CMD/MOSI functions on gpio pins
	// and GPIO19 & 23 as normal gpio
	HW_PINCTRL_MUXSEL1_SET(0x0003C3C0);
	HW_PINCTRL_MUXSEL1_CLR(0x00010100);
	// set GPIO19 as out(high), GPIO23 as in
	HW_PINCTRL_DRIVE2_CLR(0x00001000);	// set GPIO19 current
	HW_PINCTRL_DRIVE2_SET(0x00002000);	// set GPIO19 current 12mA
	HW_PINCTRL_DOUT0_SET(0x00080000);	// set GPIO19 out state high
	HW_PINCTRL_DOE0_SET(0x00080000);	// make GPIO19 output
	HW_PINCTRL_DOE0_CLR(0x00800000);	// make GPIO23 input

	HWi_SSP_CTRL0_CLR(2, BM_SSP_CTRL0_CLKGATE);
	delay(1000);

	HWi_SSP_CTRL0_CLR(2, BM_SSP_CTRL0_SFTRST);
	delay(1000);

	HWi_SSP_TIMING_WR(2, SSP2_DIV<<BP_SSP_TIMING_CLOCK_DIVIDE | SSP2_RATE<<BP_SSP_TIMING_CLOCK_RATE);

	HWi_SSP_CTRL1_CLR(2, BM_SSP_CTRL1_WORD_LENGTH|BM_SSP_CTRL1_SSP_MODE);
	HWi_SSP_CTRL1_SET(2, BV_SSP_CTRL1_WORD_LENGTH__EIGHT_BITS<<BP_SSP_CTRL1_WORD_LENGTH | BV_SSP_CTRL1_SSP_MODE__SPI<<BP_SSP_CTRL1_SSP_MODE);

	HWi_SSP_CMD0_WR(2, 0);
	HWi_SSP_CMD1_WR(2, 0);

	if (!(HW_PINCTRL_DIN0_RD()&0x00800000))
	{
		printf("GPIO23 is low!\r\n");
		return;
	}

	//printf("set GPIO19 low\r\n");
	HW_PINCTRL_DOUT0_CLR(0x00080000);	// set GPIO19 out state low

	//printf("waiting for GPIO23 to go low\r\n");
        start = HW_DIGCTL_MICROSECONDS_RD();
	while (HW_PINCTRL_DIN0_RD()&0x00800000)
		if (HW_DIGCTL_MICROSECONDS_RD() > start+500) break;
	if (HW_PINCTRL_DIN0_RD()&0x00800000)
	{
		printf("GPIO23 timeout 1\r\n");
		return;
	}

	//printf("HW_SSP2_CTRL0=0x%x\r\n", HWi_SSP_CTRL0_RD(2));
	//printf("HW_SSP2_CTRL1=0x%x\r\n", HWi_SSP_CTRL1_RD(2));

	// send 3 bytes in "correct" order
	for (i=0; i<3; i++)
	{
		data <<= 8;

		HWi_SSP_CTRL0_CLR(2, BM_SSP_CTRL0_XFER_COUNT | BM_SSP_CTRL0_READ);
		HWi_SSP_CTRL0_SET(2, 1);  // either 1 or 4 "words" to send

		//printf("setting SSP2 RUN\r\n");
		HWi_SSP_CTRL0_SET(2, BM_SSP_CTRL0_RUN);

        	start = HW_DIGCTL_MICROSECONDS_RD();
		//printf("waiting for SSP2 RUN to go high\r\n");
		while (!(HWi_SSP_CTRL0_RD(2)&BM_SSP_CTRL0_RUN))
			if (HW_DIGCTL_MICROSECONDS_RD() > start+50) break;
		if (!(HWi_SSP_CTRL0_RD(2)&BM_SSP_CTRL0_RUN))
		{
			printf("SSP2_RUN timeout 1\r\n");
			break;
		}

		//printf("HW_SSP2_DATA=0x%x\r\n", (data>>24)&0xff);
		HWi_SSP_DATA_WR(2, (data>>24)&0xff);

		//printf("setting SSP2 DATA_XFER\r\n");
		HWi_SSP_CTRL0_SET(2, BM_SSP_CTRL0_DATA_XFER);

        	start = HW_DIGCTL_MICROSECONDS_RD();
		//printf("waiting for SSP2 RUN to go low\r\n");
		while (HWi_SSP_CTRL0_RD(2)&BM_SSP_CTRL0_RUN)
			if (HW_DIGCTL_MICROSECONDS_RD() > start+50) break;
		if (HWi_SSP_CTRL0_RD(2)&BM_SSP_CTRL0_RUN)
		{
			printf("SSP2_RUN timeout 2\r\n");
			break;
		}
	}

	//printf("set GPIO19 high\r\n");
	HW_PINCTRL_DOUT0_SET(0x00080000);	// set GPIO19 out state high

	//printf("waiting for GPIO23 to go high\r\n");
        start = HW_DIGCTL_MICROSECONDS_RD();
	while (!(HW_PINCTRL_DIN0_RD()&0x00800000))
	{
		int c = agetc();
		if (c>=0) putc(c);
		if (HW_DIGCTL_MICROSECONDS_RD() > start+5000) break;
	}
	if (!(HW_PINCTRL_DIN0_RD()&0x00800000))
	{
		printf("GPIO23 timeout 2\r\n");
		return;
	}
}

void new_val(unsigned int *addr)
{
	if (get_hex(addr))
		printf("=0x%x\r\n", *addr);
	else
		printf("Abort\r\n");
}

int _start(int arg)
{
	printf("\r\nHolidayTest07\r\n");

	pinit();
	ainit();

	int c;
	unsigned int start, cur;
	start = HW_DIGCTL_MICROSECONDS_RD();
	while (1)
	{
		cur = HW_DIGCTL_MICROSECONDS_RD();
		if (cur>start+10000000)
		{
			//printf(".");
			start = cur;
		}
		c = getc();
		if (c>=0)
		{
			putc(c);
			if (c>=0x28 && c<=0x5F) aputc(c);
			else if (c=='h') i2c_write(0); //eeprom prep
			else if (c=='i') i2c_write(1); //eeprom erase
			else if (c=='j') new_val(&cust2);
			else if (c=='k') new_val(&cust3);
			else if (c=='l') new_val(&lock);
			else if (c=='m') new_val(&cust0);
			else if (c=='n') new_val(&cust1);
			else if (c=='o') dump_ocotp();
			else if (c=='p') new_val(&rom0);
			else if (c=='r') dump_ram();
			else if (c=='s') spi_test();
			else if (c=='t') mem_test();
			else if (c=='w') update_ocotp();
			else if (c=='z') reset_atmega();
			else printf("?\r\n");
		}
		c = agetc();
		if (c>=0) putc(c);
        }

	return 0;
}

