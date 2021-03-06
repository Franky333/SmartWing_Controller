#ifndef TMC_IC_TMC6200_H_
#define TMC_IC_TMC6200_H_

#include "tmc_helpers/API_Header.h"
#include "TMC6200_Register.h"
#include "TMC6200_Constants.h"

int16_t tmc6200_readInt(uint8_t drv, uint8_t address);
void tmc6200_writeInt(uint8_t drv, uint8_t address, int value);

#endif /* TMC_IC_TMC6630_H_ */
