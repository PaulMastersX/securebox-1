#ifndef __SIMPLE_QUEUE_H
#define __SIMPLE_QUEUE_H

#include <stdint.h>

typedef struct
{
  uint8_t * const buf;
  int head;
  int tail;
  const int maxlen;
} ringBuf_t;

typedef struct
{
  uint32_t * const buf;
  int head;
  int tail;
  const int maxlen;
} ringBuf32_t;

int ringBufPush(ringBuf_t *q, uint8_t data);
int ringBufPop(ringBuf_t *q, uint8_t *data);
int ringBufDelete(ringBuf_t *q);

int ringBufPush32(ringBuf32_t *q, uint32_t data);
int ringBufPop32(ringBuf32_t *q, uint32_t *data);
int ringBufSPop32(ringBuf32_t *q, uint32_t *data);
int ringBufDelete32(ringBuf32_t *q);

#endif /* __SIMPLE_QUEUE_H */
