#ifndef STM32_FLASH_COMMON_H
#define STM32_FLASH_COMMON_H

/*
 * What a family has to give to the common code: the flash accelerator is not the same in an F4 and in an F7 (the
 * F4 resets its instruction and data caches, the F7 its ART accelerator). Called after every erase and every
 * program, with the flash locked.
 */
void stm32_flash_reset_accelerator(void);

#endif // STM32_FLASH_COMMON_H
