/* state_machine.h - lightweight system/communication state machine */
#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include <stdint.h>

typedef enum {
  ST_SYS_INIT     = 0,
  ST_NODE_IDLE,        /* 终端：空闲（等待周期采集） */
  ST_NODE_COLLECT,     /* 终端：执行采集 */
  ST_NODE_REPORT,      /* 终端：上报 / 应答轮询 */
  ST_GW_IDLE,          /* 网关：空闲 */
  ST_GW_POLL,          /* 网关：轮询某节点 */
  ST_GW_WAIT_ACK,      /* 网关：等待节点应答 */
  ST_GW_UPLOAD,        /* 网关：WIFI 汇聚上传 */
} sys_state_t;

typedef struct {
  sys_state_t current;
  sys_state_t previous;
  uint8_t sub_step;     /* 当前状态内的分步 */
  uint32_t entry_tick;
} sm_ctx_t;

void sm_init(sm_ctx_t *ctx, sys_state_t init);
void sm_transit(sm_ctx_t *ctx, sys_state_t next);
sys_state_t sm_current(const sm_ctx_t *ctx);
uint8_t sm_sub(const sm_ctx_t *ctx);
void sm_sub_inc(sm_ctx_t *ctx);

#endif
