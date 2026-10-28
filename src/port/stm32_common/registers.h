#ifndef STM32_FLASH_REGISTERS_H
#define STM32_FLASH_REGISTERS_H

#include <stdint.h>

/* The flash interface of the STM32F4 (RM0090) and the STM32F7 (RM0385): the same registers, the same bits */
#ifndef REG
#define REG(address)    (*(volatile uint32_t*)(uintptr_t)(address))
#endif

#define FLASH_BASE_ADDRESS  0x40023C00u
#define FLASH_ACR           REG(FLASH_BASE_ADDRESS + 0x00u)
#define FLASH_KEYR          REG(FLASH_BASE_ADDRESS + 0x04u)
#define FLASH_SR            REG(FLASH_BASE_ADDRESS + 0x0Cu)
#define FLASH_CR            REG(FLASH_BASE_ADDRESS + 0x10u)

#define FLASH_KEY1          0x45670123u
#define FLASH_KEY2          0xCDEF89ABu

#define FLASH_SR_EOP        (1u << 0)
#define FLASH_SR_WRPERR     (1u << 4)
#define FLASH_SR_ERRORS     ((1u << 1) | (1u << 4) | (1u << 5) | (1u << 6) | (1u << 7))     /* OPERR, WRPERR, PGAERR, PGPERR, PGSERR / ERSERR */
#define FLASH_SR_BSY        (1u << 16)

#define FLASH_CR_PG         (1u << 0)
#define FLASH_CR_SER        (1u << 1)
#define FLASH_CR_SNB_SHIFT  3u
#define FLASH_CR_SNB_MASK   (0x1Fu << FLASH_CR_SNB_SHIFT)       /* (5 bits on the parts with two banks, 4 on the others) */
#define FLASH_CR_PSIZE_SHIFT 8u
#define FLASH_CR_PSIZE_MASK (3u << FLASH_CR_PSIZE_SHIFT)
#define FLASH_CR_STRT       (1u << 16)
#define FLASH_CR_LOCK       (1u << 31)

/* Cortex-M7 caches (SCB): there are none on a Cortex-M4 - the bits read as 0 and nothing is done */
#define SCB_CCR             REG(0xE000ED14u)
#define SCB_ICIALLU         REG(0xE000EF50u)
#define SCB_DCIMVAC         REG(0xE000EF5Cu)
#define SCB_CCR_DC          (1u << 16)
#define SCB_CCR_IC          (1u << 17)
#define CACHE_LINE          32u

#endif // STM32_FLASH_REGISTERS_H
