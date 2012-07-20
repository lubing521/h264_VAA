#ifndef __H27UBG8T2A__
#define __H27UBG8T2A__

//flash IO width,0-8bit,1-16bit
#define FLASH_WIDTH			0
#define FLASH_ERASE_CYCLE	3
#define FLASH_ADDR_CYCLE	5
//the value is used for loop to check if rb is timeout,the loop CPU time = 2^25 * 10 *tCLOCK
#define FLASH_RB_WAIT_COUNTER		25
//physical block amount in a die,2^11=2048
#define FLASH_PB_PER_DIE	10
//above definations are used  for low format

//sector(512B) amount in a physical page
#define FLASH_SECTOR_AMOUNT_PER_PP	16

//physical page amount in a physical block
#define FLASH_PP_AMOUNT_PER_PB	256

//physical block amount in a die,##used by NANDSim
#define FLASH_PB_AMOUNT_PER_DIE				1024

//the byte amount according to a sector in the physical page, 
//the total spare area size = FLASH_SPARE_SIZE_PER_SECTOR * FLASH_SECTOR_AMOUNT_PER_PP
#define FLASH_SPARE_SIZE_PER_SECTOR	0x10

//used CE amount in current system,##used by NANDSim
#define CE_AMOUNT						1

//how many valid bits are in the 5th cycle, it will decide the file amount will be created,##used by NANDSim
#define HIGHEST_CYCLE_VALUE				(3+1)

#endif
