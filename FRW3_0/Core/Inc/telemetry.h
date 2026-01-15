#ifndef TELEMETRY_H_INCLUDED
#define TELEMETRY_H_INCLUDED

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void Telemetry_Init(void);
void Telemetry_Update(void);
void Telemetry_SetFrameBufferAddress(uint32_t fb_addr);

#ifdef __cplusplus
}
#endif

#endif /* TELEMETRY_H */
