#pragma once

#include <stdint.h>

extern uint8_t UBRRH, UBRRL, UCSRB, UCSRC, TCCR2, OCR2, TCNT2, TIMSK, UDR;
enum { RXEN, RXCIE, URSEL, UCSZ1, UCSZ0, WGM21, CS22, OCIE2 };
