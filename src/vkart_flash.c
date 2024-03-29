#include "ch32v30x.h"
#include "vkart_flash.h"
#include "SW_UDISK.h"


/*
 * MX commands
 * 
 * 000,F0			Reset
 * 555,AA 2AA,55 555,90		Manufacture ID, Device ID, Sect Fact, Sect Prot
 * 555,AA 2AA,55 555,88		Enter Security region enable
 * 555,AA 2AA,55 555,90 xxx,00  Exit Security sector
 * 555,AA 2AA,55 555,A0		Program
 * 555,AA 2AA,55 555,80 555,AA 2AA,55 555,10	Chip Erase
 * 555,AA 2AA,55 555,80 555,AA 2AA,55 sct,30	Sector sct erase
 *  55,98			CFI read
 * xxx,B0 			Erase suspend
 * xxx,30			Erase resume
 *
 *
 *
 * ST commands
 * 22ED (top),22FD (bottom)
 *
 * 000,F0					Reset
 * 555,AA 2AA,55 000,F0		Reset
 * 555,AA 2AA,55 555,90		Auto Select
 * 555,98					CFI read
 * 55,AA 2AA,55 555,A0 PA,PD	Program
 * 555,50 PA0,PD0 PA1,PD1		Double Program
 * 555,56 PA0,PD0 PA1,PD1, PA2,PD2 PA3 PD3	Quadruple Program
 * 555,AA 2AA,55 555,20		Unlock ByPass
 * 000,A0 PA,PD				Unlock ByPass Program
 * 000,90 000,00			Unlock ByPass Reset
 * 555,AA 2AA,55 55,555 555,80 555,AA 2AA,55 555,10	Chip Erase
 * 555,AA 2AA,55 555,80 555,AA 2AA,55 BlkA,30	Block Erase
 * 000,B0					Program Erase Suspend
 * 000,30					Program Erase Resume
 * 555,AA 2AA,55 555,88		Enter Extended Block
 * 555,AA 2AA,55 555,90 000,00	Exit Extended Block
 */

#define DATA_READ 	0
#define DATA_WRITE  	1

#define SET_MASK(A, B, mask) ( (A & ~mask) | (B & mask) )

enum {
	REGULAR = 0,
	BOTTOM,
	TOP
} layout_type;

enum {
	CLEAN = 1,
	DIRTY,
	UNKNOWN
};

typedef enum {
		Success,
		AddressInvalid,
		EraseFailed,
		ProgramFailed,
		ProgramAbort,
		ToggleFailed,
		OperationTimeOut,
		InProgress,
		Cmd_Invalid,
		SectorProtected,
		SectorUnprotected
} ret_msg;

uint8_t vkart_sectors[135];
uint8_t flash_layout = REGULAR;
int8_t top_bottom = -1;
int8_t support_double = 0;
uint16_t device_id;


void VKART_Init(void) {
    //GPIO_InitTypeDef  GPIO_InitStructure = {0};
    RCC_APB2PeriphClockCmd(
        RCC_APB2Periph_USART1 | 
        RCC_APB2Periph_GPIOA |
        RCC_APB2Periph_GPIOD | 
        RCC_APB2Periph_GPIOC |
        RCC_APB2Periph_GPIOB | 
        RCC_APB2Periph_GPIOE, ENABLE);
    GPIOD->CFGLR = 0x44444444;
    GPIOD->CFGHR = 0x44444444;
    // Address Low
    GPIOC->CFGLR = 0x44444444;
    GPIOC->CFGHR = 0x44444444;
        
    // Adress Hi
    GPIOB->CFGHR = SET_MASK(GPIOB->CFGHR, 0x44444400, 0xffffff00);
        
    // Control
    GPIOE->CFGLR |= 0x00000333;
    GPIOE->CFGLR = 0x44444333;

    // unknown status by default
    for(int s = 0; s < 135 ; ++s) vkart_sectors[s]=UNKNOWN;

	do_reset();
	printf("device id %04x\r\n", device_id = get_device_id());
    if (device_id == 0x22cb || device_id == 0x22e9) { // Macronix
	    if (device_id == 0x22cb) {
			flash_layout = BOTTOM;
			top_bottom = 0;
		}
	    if (device_id == 0x22e9) {
			flash_layout = TOP;
			top_bottom = 127;
		}
	}
    if (device_id == 0x22ed || device_id == 0x22fd) { // ST-Numonix
	    if (device_id == 0x22fd) {
			flash_layout = BOTTOM;
			top_bottom = 0;
		}
	    if (device_id == 0x22ed) {
			flash_layout = TOP;
			top_bottom = 127;
		}
		support_double = 1;
	}
	printf("flash layout : %d.%d\r\n", flash_layout, top_bottom);
	do_reset();

}

void set_ce(uint8_t state) {
        if (state) {
                GPIOE->BSHR = 0x00000002; // 00000000 00000000 00000000 00000010
        } else {
                GPIOE->BSHR = 0x00020000; // 00000000 00000010 00000000 00000000
        }
}
void set_rw(uint8_t state) {
        if (state) {
                GPIOE->BSHR = 0x00000001;
        } else {
                GPIOE->BSHR = 0x00010000;
        }
}
void led(uint8_t state) {
        GPIOE->CFGLR |= 0x00000300;
        if (state) {
                GPIOE->BSHR = 0x00040000;
                //GPIO_WriteBit(GPIOE, GPIO_Pin_2, Bit_SET);
        } else {
                //GPIOE->CFGLR |= 0x00000300;
                GPIOE->BSHR = 0x00000004;
                //GPIO_WriteBit(GPIOE, GPIO_Pin_2, Bit_RESET);
        }
}
void set_data_dir(uint8_t state) {
        if (state) {
                GPIOD->CFGLR = 0x33333333; // output
                GPIOD->CFGHR = 0x33333333;
        } else {
                GPIOD->CFGLR = 0x44444444; // input
                GPIOD->CFGHR = 0x44444444;
        }
}
void set_address(uint32_t addr) {
        GPIOC->CFGLR = 0x33333333;
        GPIOC->CFGHR = 0x33333333;
        //GPIOB->CFGLR = 0x33333333;
        //GPIOB->CFGHR ^= 0x33333300 ^ 0xffffff00;
        GPIOB->CFGHR = SET_MASK(GPIOB->CFGHR, 0x33333300, 0xffffff00);
        GPIO_Write(GPIOC, addr);
		// 1111110000000000
		int mask = 0xfc00;
        //GPIOB->OUTDR ^= ((addr >> 16) << 10) ^ 0xfc00;
		//GPIOB->OUTDR = (GPIOB->OUTDR & ~mask) | (((addr>>16)<<10) & mask);
		GPIOB->OUTDR = SET_MASK(GPIOB->OUTDR, (addr >> 16) << 10, mask);
		//out = (in & ~mask) | (val & mask);
}
void set_data(uint16_t data) {
        GPIO_Write(GPIOD, data);
}
uint16_t get_data(void) {
        return GPIO_ReadInputData(GPIOD);
}
void write_word(uint32_t addr, uint16_t word) {
        set_ce(1);
        set_rw(0);
        set_data_dir(DATA_WRITE);
        set_address(addr);
        set_ce(0);
        set_data(word);
        set_ce(1);
}
void write_word_mx2(uint32_t addr, uint16_t d1) {
		do_reset();
}

ret_msg data_toggle(void) {
	uint8_t tog1, tog2;

	tog1 = read_word(0);
	tog2 = read_word(0);

	if ((tog1 & 0x40) == (tog2 & 0x40)) {
			return Success;
	}
	if ((tog2 & 0x20) != 0x20) {
			return InProgress;
	}
	tog1 = read_word(0);
	tog2 = read_word(0);
	if ((tog1 & 0x40) == (tog2 & 0x40)) {
			return Success;
	} else {
			return ToggleFailed;
	}
}


void write_word_mx(uint32_t addr, uint16_t d1) {
		uint8_t t1,t2;
    //if (d1) printf("writing %04x at %08x\r\n", d1, addr);
//	if (d1 != 0xffff) {
		write_word(0x0555,0xAA);
		write_word(0x02AA,0x55);
		write_word(0x0555,0xA0);
		write_word(addr, d1);
		//Delay_Us(13); // typical 11us
		while (1) {
				t1 = read_word(0);
				t2 = read_word(0);
				//printf("%d %d\r\n", t1, t2);
				if ((t1 & 0x40) == (t2 & 0x40)) break;
		}
//	}
    //check_status();
}
void write_word_29w(uint32_t addr, uint16_t d1, uint16_t d2) {
    write_word(0x0555,0x0050);
    write_word(addr, d1);
    write_word(addr+1, d2);
	//while ((read_word(0)&0x40) != (read_word(0)&0x40));
    Delay_Us(20); // typical 10us
}
void erase_block(uint32_t addr) {
		do_reset();
		write_word(0x0555, 0xaa);
		write_word(0x02aa, 0x55);
		write_word(0x0555, 0x80);
		write_word(0x0555, 0xaa);
		write_word(0x02aa, 0x55);
		write_word(addr, 0x30);
			
		uint8_t q = 0;
		/*
		set_data_dir(DATA_READ);
		while (1) {
			q = read_word(addr);
			if ((q & 0x08) != 0) break;
		}
		while (1) {
			q = read_word(addr);

			if ((q & 0x80) != 0) {
				// Finished
				break;
			}
		} */
		Delay_Ms(900);
		printf("End erase block %08lx st=%02x\r\n", addr, q);
}
/* void erase_chip() {
    write_word(0x555, 0xaa);
    write_word(0x2aa, 0x55);
    write_word(0x555, 0x80);
    write_word(0x555, 0xaa);
    write_word(0x2aa, 0x55);
    write_word(0x555, 0x10);
    Delay_Ms(80000); // typical 80s
    printf("chip erased in %lu cycles", check_status());
} */

uint16_t read_word(uint32_t addr) {
        uint16_t ret;
        set_ce(1);
        set_rw(1);
        set_address(addr);
        set_ce(0);
        ret = get_data();
		//printf("read %04x\r\n", ret);
        return ret;
}
uint16_t get_device_id() {
        printf("-- get_device_id --\r\n");
        write_word(0x555, 0xAA);
        write_word(0x2AA, 0x55);
        write_word(0x555, 0x90);
		set_data_dir(DATA_READ);
        return read_word(0x1);
}
void do_reset() {
        write_word(0x0, 0x00f0);
}

void vkart_read_data(uint32_t addr, uint16_t *buff, uint32_t len) {
    
    set_data_dir(DATA_READ);
    for(int i = 0; i < len; ++i) {
        *buff++ = read_word(addr++);
    }
}

void vkart_erase_sector(uint32_t addr, uint8_t block) {
	printf("erase sector addr %08lx for %d\r\n", addr, block);
	erase_block(addr);
	vkart_sectors[block] = CLEAN;
	do_reset();
}

void display_blocks() {
	int s = 0;
	char c = 0;
	for(int i = 0 ; i < 8; i++) {
		for(int j = 0; j < 17; j++) {
				s = vkart_sectors[i*17 + j];
				if (s == UNKNOWN) c = '?';
				if (s == CLEAN) c = 'c';
				if (s == DIRTY) c = 'X';
				printf("%c ", c);
		}
		printf("\r\n");
	}
}
void vkart_debug(void) {
		printf("debug function");
}

void vkart_write_block2(uint16_t *pbuf, uint32_t addr, uint32_t len) {
	printf("vkart_write_block at %08lx %ld words\r\n", addr, len);
	for(int i = 0; i < len; i++) {
		//if (i % 16 == 0) printf("%08lx: ", (addr+i));
		//printf("%04x ",  pbuf[i]);
		//if (i % 16 == 15) printf("\r\n");
		//printf("prog %08lx with %04x\r\n", addr+i, pbuf[i]);
		write_word_mx(addr+i, pbuf[i]);
	}

}
void vkart_write_block(uint16_t *pbuf, uint32_t addr, uint32_t len) {
	uint8_t dirty = 0;
    uint8_t block = addr >> 15;
	uint32_t block_len = 0x8000;
	//if (addr > 0x3000) return;
	printf("vkart_write_block at %08lx %ld words\r\n", addr, len);

	if (flash_layout == BOTTOM) {
		if (block == top_bottom) {
				block = addr >> 12;
				block_len = 0x1000;
		} else {
				block += 7;
		}
	}
	if (flash_layout == TOP) {
		if (block == top_bottom) {
			block += (addr >> 12) & 7;
			block_len = 0x1000;
		}
	}
	uint8_t same = 0;
	for(int i = 0; i < len; i++) {
		same = (read_word(addr+i) == pbuf[i]);
		if (!same) break;
	}
	if (same) {
		printf("same buffer at %08lx\r\n", addr);
		return;
	}

	if (vkart_sectors[block] != CLEAN) {
		uint16_t w;
		for(int i = 0; i < block_len; i++) {
			w = read_word(addr + i);
			if (w != 0xffff) {
				printf("dirty word %04x at pos %d in block %d\r\n", w, i, block);
				dirty = 1;
				break;
			}
		}
		if (dirty) {
			printf("block %d is dirty\r\n", block);
			vkart_sectors[block] = DIRTY;
		} else {
			printf("block %d is clean\r\n", block);
			vkart_sectors[block] = CLEAN;
		}
	}
	if (vkart_sectors[block] == DIRTY) {
		printf("vkart_erase_sector(%lx, %x)\r\n", addr, block);
		vkart_erase_sector(addr, block);
		vkart_sectors[block] = CLEAN;
	}
	//display_blocks();
	if (support_double) {
			printf("double\r\n");
			for(int i = 0; i < len; i+=2) {
				//if (i % 16 == 0) printf("%08lx: ", (addr+i));
				//printf("%04x ",  pbuf[i]);
				//if (i % 16 == 15) printf("\r\n");
				//printf("prog %08lx with %04x\r\n", addr+i, pbuf[i]);
				//printf("prog %08lx with %04x\r\n", addr+i, pbuf[i]);
				write_word_29w(addr+i, pbuf[i], pbuf[i+1]);
			}
	} else {
			printf("single\r\n");
			for(int i = 0; i < len; i++) {
				write_word_mx(addr+i, pbuf[i]);
			} 
	}
	printf("prog %ld words done at %08lx\r\n", len, addr);
    
}
