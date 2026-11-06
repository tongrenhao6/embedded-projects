/* state_machine.c */
#include "state_machine.h"
#include "main.h"

void sm_init(sm_ctx_t *ctx, sys_state_t init)
{
  ctx->current   = init;
  ctx->previous  = init;
  ctx->sub_step  = 0U;
  ctx->entry_tick = HAL_GetTick();
}

void sm_transit(sm_ctx_t *ctx, sys_state_t next)
{
  ctx->previous = ctx->current;
  ctx->current  = next;
  ctx->sub_step = 0U;
  ctx->entry_tick = HAL_GetTick();
}

sys_state_t sm_current(const sm_ctx_t *ctx)
{
  return ctx->current;
}

uint8_t sm_sub(const sm_ctx_t *ctx)
{
  return ctx->sub_step;
}

void sm_sub_inc(sm_ctx_t *ctx)
{
  ctx->sub_step++;
}
