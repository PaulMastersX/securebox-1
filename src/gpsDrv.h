#ifndef __GPS_DRV_H
#define __GPS_DRV_H
#include <stdint.h>

typedef struct {
  uint8_t *str;
  uint32_t len;
} gpsDrvIN_frame;

#define GPSDRV_BUFIN_SZ 1000
#define GPSDRV_BUFOUT_SZ 1000

void gpsParse(uint8_t data);
int32_t gpsDrvIN_read(gpsDrvIN_frame **addr);
int32_t gpsDrvIN_write(uint8_t data);
void gpsDrv_Setup(void);

#endif /* __GPS_DRV_H */