#ifndef TMC6200_H_
#define TMC6200_H_

#include <stdint.h>

#define WRITE 0x80
#define READ 0x00

void tmc6200_init(uint8_t drv);

#endif /* TMC6200_H_ */
