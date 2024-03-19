/********************************** (C) COPYRIGHT *******************************
* File Name          : main.c
* Author             : WCH
* Version            : V1.0.0
* Date               : 2021/06/06
* Description        : Main program body.
*********************************************************************************
* Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for 
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/

/* @Note
 * UDisk Example:
 * This program provides examples of UDisk.Supports external SPI Flash and internal
 * Flash, selected by STORAGE_MEDIUM at SW_UDISK.h.
 *  */

#include "ch32v30x_usbhs_device.h"
#include "debug.h"
#include "Internal_Flash.h"
#include "SPI_FLASH.h"
#include "SW_UDISK.h"
#include "vkart_flash.h"

/*********************************************************************
 * @fn      main
 *
 * @brief   Main program.
 *
 * @return  none
 */
int main(void)
{
	SystemCoreClockUpdate( );
	Delay_Init( );
	USART_Printf_Init( 115200 );
		
	printf( "SystemClk:%ld\r\n",SystemCoreClock );
	//printf( "ChipID:%08x\r\n", DBGMCU_GetCHIPID() );
	printf( "This program is a Simulate UDisk\r\n" );
	
	Delay_Ms(10);
#if (STORAGE_MEDIUM == MEDIUM_SPI_FLASH)
    printf( "USBHS UDisk Demo\r\nStorage Medium: SPI FLASH \r\n" );
    /* SPI flash init */
    FLASH_Port_Init( );
    /* FLASH ID check */
    FLASH_IC_Check( );
#elif (STORAGE_MEDIUM == MEDIUM_INTERAL_FLASH)
    printf( "USBHS UDisk Demo\r\nStorage Medium: INTERNAL FLASH \r\n" );
    Flash_Sector_Count = IFLASH_UDISK_SIZE  / DEF_UDISK_SECTOR_SIZE;
    Flash_Sector_Size = DEF_UDISK_SECTOR_SIZE;

#elif (STORAGE_MEDIUM == MEDIUM_VKART_FLASH)
	printf("USBD Udisk\r\nStorage Medium: VKART Flash\r\n");
	VKART_Init();
	Flash_Sector_Count = VKART_UDISK_SIZE / DEF_UDISK_SECTOR_SIZE;
	Flash_Sector_Size = DEF_UDISK_SECTOR_SIZE;
	// VKART_UDISK_END_ADDR		0x7FFFFF
	// VKART_UDISK_START_ADDR	0x0
	// VKART_UDISK_SIZE		(VKART_UDISK_END_ADDR - VKART_UDISK_START_ADDR + 1) : 0x800000 (8388608)
	
#endif

    /* Enable Udisk */
    Udisk_Capability = Flash_Sector_Count;
    Udisk_Status |= DEF_UDISK_EN_FLAG;

    /* USB20 device init */
    USBHS_RCC_Init( );
    USBHS_Device_Init( ENABLE );

	while(1)
	{
	    ;
	}
}