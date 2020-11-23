/* 
  i2sTXcircular.h - Software I2S library for esp8266
  
  Modified i2s.c to just give continuous output from a circular queue of buffers
*/
#include "Arduino.h"
#include "osapi.h"
#include "ets_sys.h"

#include "i2s_reg.h"
#include "i2sTXcircular.h"

extern void ets_wdt_enable(void);
extern void ets_wdt_disable(void);

static uint16_t slcCount32; //Total buffer size
static uint16_t slcBufCnt; //Number of buffers in the I2S circular buffer
static uint16_t slcBufLen; //Length of one buffer, in 32-bit words.
static uint16_t slcBufRemainder; // extra 32 bit words in last buffer
static uint32_t slcBitClock;
static uint16_t slcOneShot;
static uint32_t debugVal[4];

struct slc_queue_item {
	uint32  blocksize:12;
	uint32  datalen:12;
	uint32  unused:5;
	uint32  sub_sof:1;
	uint32  eof:1;
	uint32  owner:1;
	uint32  buf_ptr;
	uint32  next_link_ptr;
};
#define MAX_BUFFERDATA 4096
#define MAX_QUEUE 64
static uint32_t i2scBufferData[MAX_BUFFERDATA];
static struct slc_queue_item i2scSlc_items[MAX_QUEUE]; //I2S DMA buffer descriptors
static bool (*fillBuffers) (void) = i2scFillBuffersMS;

//mark space pattern definition
// each record defines a number of mark bits followed by a number of space bits and a repeat count
// if repeat count is 0 then this pattern is just repeated to the end.
#define FILL_MAX 100
#define FILL_SPACEBITS 0
#define FILL_MARKBITS 1
#define FILL_REPEAT 2
static uint16 markspaceSize = 0;
static uint16 markspaceBits[FILL_MAX][3];

//This routine is called as soon as the DMA routine has something to tell us. All we
//handle here is the RX_EOF_INT status, which indicate the DMA has sent a buffer whose
//descriptor has the 'EOF' field set to 1.
void ICACHE_FLASH_ATTR i2scSlc_isr(void) {
	uint32_t slc_intr_status = SLCIS;
	SLCIC = 0xFFFFFFFF;
	if (slc_intr_status & SLCIRXEOF && slcOneShot) {
		ETS_SLC_INTR_DISABLE();
		i2scSlc_end();
	}
}

void ICACHE_FLASH_ATTR i2scSlc_begin() {
	int x, y;
	// Clean up any running I2S, free all buffers
	i2scEnd();

	for (x=0; x<slcBufCnt; x++) {
		i2scSlc_items[x].unused = 0;
		i2scSlc_items[x].owner = 1;
		i2scSlc_items[x].eof = 0;
		i2scSlc_items[x].sub_sof = 0;
		i2scSlc_items[x].datalen = slcBufLen*4;
		i2scSlc_items[x].blocksize = slcBufLen*4;
		i2scSlc_items[x].buf_ptr = (uint32_t)&i2scBufferData[x*slcBufLen];
		i2scSlc_items[x].next_link_ptr = (int)((x<(slcBufCnt-1))?(&i2scSlc_items[x+1]):(&i2scSlc_items[0]));
	}
	i2scSlc_items[slcBufCnt-1].eof = 1;
	i2scSlc_items[slcBufCnt-1].datalen += slcBufRemainder * 4;

	ETS_SLC_INTR_DISABLE();
	SLCC0 |= SLCRXLR | SLCTXLR;
	SLCC0 &= ~(SLCRXLR | SLCTXLR);
	SLCIC = 0xFFFFFFFF;

	//Configure DMA
	SLCC0 &= ~(SLCMM << SLCM); //clear DMA MODE
	SLCC0 |= (1 << SLCM); //set DMA MODE to 1
	SLCRXDC |= SLCBINR | SLCBTNR; //enable INFOR_NO_REPLACE and TOKEN_NO_REPLACE
	SLCRXDC &= ~(SLCBRXFE | SLCBRXEM | SLCBRXFM); //disable RX_FILL, RX_EOF_MODE and RX_FILL_MODE

	//Feed DMA the 1st buffer desc addr
	//To send data to the I2S subsystem, counter-intuitively we use the RXLINK part, not the TXLINK as you might
	//expect. The TXLINK part still needs a valid DMA descriptor, even if it's unused: the DMA engine will throw
	//an error at us otherwise. Just feed it any random descriptor.
	SLCTXL &= ~(SLCTXLAM << SLCTXLA); // clear TX descriptor address
	SLCTXL |= (uint32)&i2scSlc_items[1] << SLCTXLA; //set TX descriptor address. any random desc is OK, we don't use TX but it needs to be valid
	SLCRXL &= ~(SLCRXLAM << SLCRXLA); // clear RX descriptor address
	SLCRXL |= (uint32)&i2scSlc_items[0] << SLCRXLA; //set RX descriptor address

	ETS_SLC_INTR_ATTACH(i2scSlc_isr, NULL);
	SLCIE = SLCIRXEOF; //Enable only for RX EOF interrupt

	if(slcOneShot) {
		ETS_SLC_INTR_ENABLE();
	}
	//Start transmission
	SLCTXL |= SLCTXLS;
	SLCRXL |= SLCRXLS;
}

void ICACHE_FLASH_ATTR i2scSlc_end() {
	ETS_SLC_INTR_DISABLE();
	SLCIC = 0xFFFFFFFF;
	SLCIE = 0;
	SLCTXL &= ~(SLCTXLAM << SLCTXLA); // clear TX descriptor address
	SLCRXL &= ~(SLCRXLAM << SLCRXLA); // clear RX descriptor address
}

void i2scWriteSample(uint16_t bufIx, uint32_t sample) {
	i2scBufferData[bufIx] = sample;
}

void i2scSetMSArrayItem(uint16_t MSarrayIx, uint16_t markBits, uint16_t spaceBits, uint16_t repeat) {
	if(MSarrayIx < FILL_MAX) {
		markspaceBits[MSarrayIx][FILL_MARKBITS] = markBits;
		markspaceBits[MSarrayIx][FILL_REPEAT] = repeat;
		if((markBits + spaceBits) == 0) {
			// ensure a definition of 0 mark and space doesn't cause an endless loop
			markspaceBits[MSarrayIx][FILL_SPACEBITS] = 16;
		} else {
			markspaceBits[MSarrayIx][FILL_SPACEBITS] = spaceBits;
		}
		markspaceSize = MSarrayIx + 1;
	} else {
		markspaceSize = 0;
	}
}

void i2scClearBuffers() {
	int x;
	for (x=0; x<MAX_BUFFERDATA; x++) i2scBufferData[x] = 0;
}

// fill buffers with pattern defined by markspaceBits array
bool i2scFillBuffersMS() {
	uint16_t markspaceIx = 0;
	uint16_t bufIx = 0;
	uint32_t sample;
	uint16_t sampleB = 0;
	uint16_t level = 1;
	uint16_t bitCount = markspaceBits[markspaceIx][level];
	uint16_t repeatCount = 0;

	while(bufIx < slcCount32) {
		if(bitCount == 0) {
			level ^= 1;
			if(level) {
				if(markspaceBits[markspaceIx][FILL_REPEAT]) {
					repeatCount++;
					if(repeatCount >=  markspaceBits[markspaceIx][FILL_REPEAT]) {
						repeatCount = 0;
						if(markspaceIx < (markspaceSize - 1)) {
							markspaceIx++;
						}
					}
				}
			} 
			bitCount = markspaceBits[markspaceIx][level];
		} else {
			sample = (sample << 1) | level;
			bitCount--;
			sampleB++;
			if(sampleB == 32) {
				sampleB = 0;
				i2scWriteSample(bufIx, sample);
				bufIx++;
			}
		}
	}
	return true;
}

// Set clock either by using supplied dividers or finding best ones from clockDiv1 if div2 < 0
void i2scSetBitClock(uint32_t clockDiv1, int16_t div2) { //bitClock in HZ
	if(div2 < 0) {
		// find best dividers
		slcBitClock = clockDiv1;
		clockDiv1 = 1;
		div2 = 1;
		float delta_best = I2SBASEFREQ;
		for (uint8_t i=63; i>0; i--) {
			for (uint8_t j=i; j>0; j--) {
				float new_delta = fabs(((float)I2SBASEFREQ/i/j) - slcBitClock);
				if (new_delta < delta_best) {
					delta_best = new_delta;
					clockDiv1 = i;
					div2 = j;
				}
			}
		}
	} else {
		slcBitClock = I2SBASEFREQ / clockDiv1 / div2;
	}

	// Ensure dividers fit in bit fields
	clockDiv1 &= I2SBDM;
	div2 &= I2SCDM;
	uint32_t i2sc_temp = I2SC;
	i2sc_temp |= (I2STXR); // Hold transmitter in reset
	I2SC = i2sc_temp;
	// trans master(active low), recv master(active_low), !bits mod(==16 bits/chanel), clear clock dividers
	i2sc_temp &= ~(I2STSM | I2SRSM | (I2SBMM << I2SBM) | (I2SBDM << I2SBD) | (I2SCDM << I2SCD));
	// I2SRF = Send/recv right channel first (? may be swapped form I2S spec of WS=0 => left)
	// I2SMR = MSB recv/xmit first
	// I2SRMS, I2STMS = 1-bit delay from WS to MSB (I2S format)
	// div1, div2 = Set I2S WS clock frequency.  BCLK seems to be generated from 32x this
	i2sc_temp |= I2SRF | I2SMR | I2SRMS | I2STMS | (clockDiv1 << I2SBD) | (div2 << I2SCD);
	I2SC = i2sc_temp;
	i2sc_temp &= ~(I2STXR); // Release reset
	I2SC = i2sc_temp;
}

float i2scGetRealBitClock() {
	return (float)I2SBASEFREQ/((I2SC>>I2SBD) & I2SBDM)/((I2SC >> I2SCD) & I2SCDM);
}

// set up parameters for i2scircular
bool i2scInit(uint16_t count32, uint32_t clockDiv1, int16_t div2, uint16_t oneShot) {
	if(count32 > MAX_BUFFERDATA || count32 < 32) {
		return false;
	}
	i2scSlc_end(); // make sure it is stopped
	slcCount32 = count32;
	slcBufCnt = 2;
	slcBufLen = count32 / 2;
	slcBufRemainder = 0;
	while(slcBufCnt < (MAX_QUEUE / 2) && slcBufLen > 32) {
		slcBufCnt <<= 1;
		slcBufLen >>= 1;
	}
	slcBufRemainder = count32 - slcBufCnt * slcBufLen;
	slcOneShot = oneShot;
	i2scSetBitClock(clockDiv1, div2);
}

//start up i2s, init and set clock before this
bool i2scBegin() {
	i2scSlc_begin();
	if(markspaceSize == 0) {
		//default fill pattern if not defined
		markspaceSize++;
		markspaceBits[0][FILL_REPEAT] = 0;
		markspaceBits[0][FILL_MARKBITS] = 16;
		markspaceBits[0][FILL_SPACEBITS] = 16;
	}
	if(!fillBuffers()) {
		i2scSlc_end();
		return false;
	}

	pinMode(2, FUNCTION_1); //I2SO_WS (LRCK)
	pinMode(3, FUNCTION_1); //I2SO_DATA (SDIN)
	pinMode(15, FUNCTION_1); //I2SO_BCK (SCLK)

	I2S_CLK_ENABLE();
	I2SIC = 0x3F;
	I2SIE = 0;

	//Reset I2S
	I2SC &= ~(I2SRST);
	I2SC |= I2SRST;
	I2SC &= ~(I2SRST);

	I2SFC &= ~(I2SDE | (I2STXFMM << I2STXFM) | (I2SRXFMM << I2SRXFM)); //Set RX/TX FIFO_MOD=0 and disable DMA (FIFO only)
	I2SFC |= I2SDE; //Enable DMA
	I2SCC &= ~((I2STXCMM << I2STXCM) | (I2SRXCMM << I2SRXCM)); //Set RX/TX CHAN_MOD=0
	I2SC |= I2STXS; //Start transmission
	return true;
}

void ICACHE_FLASH_ATTR i2scEnd(){
	I2SC &= ~I2STXS;

	//Reset I2S
	I2SC &= ~(I2SRST);
	I2SC |= I2SRST;
	I2SC &= ~(I2SRST);

	pinMode(2, INPUT);
	pinMode(3, INPUT);
	pinMode(15, INPUT);

	i2scSlc_end();
}

void i2scSet_fillBuffers(bool (*fB) (void)) {
	fillBuffers = fB;
}

uint32_t i2scGetDebugVal(uint16_t dbIx) {
	return debugVal[dbIx];
}