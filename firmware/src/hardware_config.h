#pragma once

#define F_CPU 16000000UL
#define BAUD 31250UL
#define MY_UBRR ((F_CPU / (16UL * BAUD)) - 1)
