#pragma once

#include <stddef.h>
#include <stdint.h>

uint8_t eeprom_read_byte(const uint8_t* address);
void eeprom_read_block(void* dest, const void* address, size_t size);
void eeprom_update_byte(uint8_t* address, uint8_t value);
void eeprom_update_block(const void* src, void* address, size_t size);
