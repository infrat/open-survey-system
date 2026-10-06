#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>

// printf-style logging to Serial, mirrored to the Telnet client when one is set.
// Signature matches TransportLogCallback / UARTLogCallback so it can be passed
// to them directly.
void logPrint(const char *format, ...);
void logPrintln(const char *format, ...);

// Set by the Telnet service while a client is connected, nullptr otherwise
void logSetMirror(Print *mirror);

#endif // LOGGER_H
