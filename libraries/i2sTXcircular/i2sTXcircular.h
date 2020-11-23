/* 
  i2sTXcircular.h - Software I2S library for esp8266
  
  
  Modified i2s.h to just give continuous output from a circular queue of buffers
*/
#ifndef I2S_h
#define I2S_h

#ifdef __cplusplus
extern "C" {
#endif

bool i2scInit(uint16_t count32, uint32_t clockDiv1, int16_t div2, uint16_t oneShot);
bool i2scBegin();
void i2scEnd();
void i2scSetBitClock(uint32_t clockDiv1, int16_t div2);
float i2scGetRealBitClock();
void i2scWriteSample(uint16_t buffIx, uint32_t sample);
void i2scSetMSArrayItem(uint16_t MSarrayIx, uint16_t markBits, uint16_t spaceBits, uint16_t repeat);
bool i2scFillBuffersMS();
void i2scClearBuffers();
void i2scSetFillBuffers(bool (*fB) (void));
uint32_t i2scGetDebugVal(uint16_t dbIx);
#ifdef __cplusplus
}
#endif

#endif
