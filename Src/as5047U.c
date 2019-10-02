#include "as5047U.h"
#include "swdriver.h"
#include "spi.h"
#include "usart.h"
#include <limits.h>
#include <string.h>

static void spiMode_set(uint8_t drv)
{
	swdriver[drv].SPI->Init.CLKPolarity = SPI_POLARITY_LOW;
	swdriver[drv].SPI->Init.CLKPhase = SPI_PHASE_2EDGE;

	if (HAL_SPI_Init(swdriver[drv].SPI) != HAL_OK)
	{
		_Error_Handler(__FILE__, __LINE__);
	}
	HAL_Delay(2);
}

static void spiMode_reset(uint8_t drv)
{
	swdriver[drv].SPI->Init.CLKPolarity = SPI_POLARITY_HIGH;
	swdriver[drv].SPI->Init.CLKPhase = SPI_PHASE_2EDGE;

	if (HAL_SPI_Init(swdriver[drv].SPI) != HAL_OK)
	{
		_Error_Handler(__FILE__, __LINE__);
	}
	HAL_Delay(2);
}

static uint8_t gencrc(uint8_t *data, uint8_t len)
{
    uint8_t crc =0xb7;
    size_t i, j;
    for (i = 0; i < len; i++)
		{
        crc ^= data[i];
        for (j = 0; j < 8; j++)
				{
            if ((crc & 0x80) != 0)
                crc = (uint8_t)((crc << 1) ^ 0x1d);
            else
                crc <<= 1;
        }
    }
    return crc;
}


static void diag(uint8_t drv, uint8_t warning, uint8_t error)
{
	static char string[500];
	uint32_t data32 = 0;
	uint16_t data16 = 0;

	sprintf(string, "\n\r\n\r");

	if (warning)
		strcat (string,"WARNING\n\r");
	if(error)
		strcat (string,"ERROR\n\r");

	as5047U_sendCommand(drv, AS5047U_READ, ADDR_ERRFL);
	data32 = as5047U_sendCommand(drv, AS5047U_READ, ADDR_ERRFL);
	data16 =(uint16_t) ((data32 & 0x003FFF00)>>8);

	if((data16>>0)&0x01)
		 strcat (string,"AGC-warning\n\r");
	else if((data16>>1)&0x01)
		strcat (string,"MagHalf\n\r");
	else if((data16>>2)&0x01)
		strcat (string,"P2ram_warning\n\r");
	else if((data16>>3)&0x01)
		strcat (string,"P2ram_error\n\r");
	else if((data16>>4)&0x01)
		strcat (string,"Framing error\n\r");
	else if((data16>>5)&0x01)
		strcat (string,"Command error\n\r");
	else if((data16>>6)&0x01)
		strcat (string,"CRC error\n\r");
	else if((data16>>7)&0x01)
		strcat (string,"WDTST\n\r");
	else if((data16>>9)&0x01)
		strcat (string,"OffCompNotFinished\n\r");
	else if((data16>>10)&0x01)
		strcat (string,"CORDIC_Overflow\n\r");

	HAL_UART_Transmit_IT(&huart3, (uint8_t*)string, 500);
	HAL_Delay(10);
}





uint32_t as5047U_sendData(uint8_t drv, uint16_t data)
{
	return as5047U_sendCommand(drv, AS5047U_WRITE, data);
}


uint16_t as5047U_getData(uint8_t drv, uint32_t data)
{
	uint8_t warning 	=  ((uint32_t)(data & 0x00800000)>>23);
	uint8_t error 		=  ((uint32_t)(data & 0x00400000)>>22);

	if(warning || error)
		diag(drv, warning, error);

	return (uint16_t) ((data & 0x003FFF00)>>8);
}



uint32_t as5047U_sendCommand(uint8_t drv, uint8_t rw, uint16_t address)
{
	uint16_t data_send = 0;
	uint32_t data_receive = 0;

	uint8_t txData[3];
	uint8_t rxData[3];

	address &= 0x3FFF;

	data_send = address | (rw<<14);


	txData[0] = (uint8_t)( (data_send & 0xff00) >> 8);
	txData[1] = (uint8_t)(data_send & 0x00ff) ;
	txData[2] = gencrc(txData, 2);

	// static char string2[50];
	// uint16_t len = snprintf(string2, 50, "\n\rsend:   %x %x %x", txData[0], txData[1], txData[2]);
	// HAL_UART_Transmit_IT(&huart3, (uint8_t*)string2, len);
	// HAL_Delay(2);

	spiMode_set(drv);
	HAL_Delay(1);
	swdriver_setCsnEncoder(drv, false);
	HAL_SPI_TransmitReceive(swdriver[drv].SPI, txData, rxData, 3, HAL_MAX_DELAY);
	swdriver_setCsnEncoder(drv, true);
	HAL_Delay(1);
	spiMode_reset(drv);

	// len = snprintf(string2, 50, "\n\rrecv:   %x %x %x", rxData[0], rxData[1], rxData[2]);
	// HAL_UART_Transmit_IT(&huart3, (uint8_t*)string2, len);
	// HAL_Delay(2);


	data_receive =  ((uint32_t)rxData[0]<<16) | ((uint32_t)rxData[1]<<8) | (uint32_t)rxData[2];

	return data_receive;
}




uint16_t as5047U_getAngle(uint8_t drv) //returns 16 bit value (with 14 bit resolution)
{
	uint32_t data_received = 0;
	uint16_t data_raw = 0;
	data_received = as5047U_sendCommand(drv, AS5047U_READ, ADDR_ANGLECOM);
	data_raw = as5047U_getData(drv, data_received);
	data_received = as5047U_sendCommand(drv, AS5047U_READ, ADDR_NOP);
	data_raw = as5047U_getData(drv, data_received);
	return ((uint16_t)data_raw<<2);
}




uint16_t as5047U_getAngle_fast(uint8_t drv) //returns 16 bit value (with 14 bit resolution)
{
	spiMode_set(drv);

	uint8_t txData[2];
	txData[0] = (1 << 7) | (1 << 6) | 0x3F; // parity 1, read, address upper 6 bits
	txData[1] = 0xFF; // address lower 6 bits

	swdriver_setCsnEncoder(drv, false);
	HAL_SPI_Transmit(swdriver[drv].SPI, txData, 2, HAL_MAX_DELAY);
	swdriver_setCsnEncoder(drv, true);
	HAL_Delay(2);

	txData[0] = 0;
	txData[1] = 0;
	uint8_t rxData[2];

	swdriver_setCsnEncoder(drv, false);
	HAL_SPI_TransmitReceive(swdriver[drv].SPI, txData, rxData, 2, HAL_MAX_DELAY);
	swdriver_setCsnEncoder(drv, true);

	// static char string2[50];
	// uint16_t len = snprintf(string2, 50, "\n\rrevc8:  %x %x", rxData[0], rxData[1]);
	// HAL_UART_Transmit_IT(&huart3, (uint8_t*)string2, len);
	// HAL_Delay(10);

	spiMode_reset(drv);

	return ((((((uint16_t)rxData[0]) & 0x3F) << 8 ) | rxData[1] ) << 2);
}




void as5047U_setABIResolution14Bit(uint8_t drv)
{
	uint32_t data_received = 0;

	//uint16_t data_raw = 0;
	// static char string2[50];
	// uint16_t len;

	// data_received = as5047U_sendCommand(drv, AS5047U_READ, ADDR_NV_SETTINGS3);
	// data_received = as5047U_sendCommand(drv, AS5047U_READ, ADDR_NOP);
	// data_raw = as5047U_getData(drv, data_received);

	// len = snprintf(string2, 50, "\n\rABI-RES: %x", ((data_raw>>5)&0x7));
	// HAL_UART_Transmit_IT(&huart3, (uint8_t*)string2, len);
	// HAL_Delay(10);

	data_received = as5047U_sendCommand(drv, AS5047U_WRITE, ADDR_NV_SETTINGS3);
	as5047U_getData(drv, data_received);
	data_received = as5047U_sendData(drv, 0b10000000);
	data_received = as5047U_sendCommand(drv, AS5047U_READ, ADDR_NV_SETTINGS3);
	data_received = as5047U_sendCommand(drv, AS5047U_READ, ADDR_NOP);
	as5047U_getData(drv, data_received);

	// len = snprintf(string2, 50, "\n\rABI-RES: %x", ((data_raw>>5)&0x7));
	// HAL_UART_Transmit_IT(&huart3, (uint8_t*)string2, len);
	// HAL_Delay(10);
}


// static void itob(int x, char *buf) // print uint8_t as binary
// {
//   unsigned char *ptr = (unsigned char *)&x;
//   int pos = 0;
//   for (int i = sizeof(uint8_t) - 1; i >= 0; i--)
//     for (int j = CHAR_BIT - 1; j >= 0; j--)
//       buf[pos++] = '0' + !!(ptr[i] & 1U << j);
//   buf[pos] = '\0';
// }

// void as5047U_brutforce(uint8_t drv)
// {
// 	uint16_t i = 0;
// 	uint32_t data = 0;
//
// 	for(i=0; i<=255; i++)
// 	{
// 		data = as5047U_sendCommand(drv, AS5047U_READ, ADDR_DIAG);
// 		as5047U_getData(data);
// 		HAL_Delay(2);
// 	}
// }



// uint32_t as5047U_sendCommand2(uint8_t drv, uint8_t rw, uint16_t address)
// {
// 	uint16_t data_send = 0;
// 	uint32_t data_receive = 0;
//
// 	uint8_t txData[3];
// 	uint8_t rxData[3];
//
// 	address &= 0x3FFF;
//
// 	//data_send = address | (rw<<14) | (parity_calculate_even(address | (rw<<14))<<15);
// 	data_send = address | (rw<<14);
//
// 	txData[0] = (uint8_t)(data_send >> 8);
// 	txData[1] = (uint8_t)data_send;
// 	txData[2] = gencrc(txData, 2);
// 	//txData[2] = crc;
//
// 	// static char string1[50];
// 	// static char string2[50];
// 	// static char string3[50];
// 	// static char string4[100];
// 	// uint16_t len = 0;
// 	// uint16_t len = snprintf(string2, 50, "\n\r\n\rsend:   %x %x %x", txData[0], txData[1], txData[2]);
// 	// HAL_UART_Transmit_IT(&huart3, (uint8_t*)string2, len);
// 	// HAL_Delay(2);
//
// 	spiMode_set(drv);
// 	HAL_Delay(2);
// 	swdriver_setCsnEncoder(drv, false);
// 	HAL_SPI_Transmit(swdriver[drv].SPI, txData, 3, HAL_MAX_DELAY);
// 	swdriver_setCsnEncoder(drv, true);
// 	HAL_Delay(2);
// 	spiMode_reset(drv);
//
// 	// itob(rxData[0], string1);
// 	// itob(rxData[1], string2);
// 	// itob(rxData[2], string3);
//
// 	// len = snprintf(string4, 100, "\n\r%s%s%s", string1, string2, string3);
// 	// HAL_UART_Transmit_IT(&huart3, (uint8_t*)string4, len);
// 	// HAL_Delay(2);
//
// 	data_receive =  ((uint32_t)rxData[0]<<16) | ((uint32_t)rxData[1]<<8) | (uint32_t)rxData[2];
//
// 	return data_receive;
// }