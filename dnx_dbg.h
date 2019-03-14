#ifndef __DNX_DBG_H__
#define __DNX_DBG_H__


#include <linux/kernel.h>


struct dnx_device;


void dnx_debug_irq(struct dnx_device *dnx, u32 irq_state);
void dnx_debug_reg_dump(struct dnx_device *dnx);
void dnx_debug_stream_err(struct dnx_device *dnx);


#endif
