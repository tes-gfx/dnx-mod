#include "dnx_selftest.h"
#include "dnx_drv.h"
#include "dnx_gpu.h"

#include "nx_register_address.h"


static void trigger_irq(struct dnx_device *dnx, u32 irq)
{
	unsigned long flags;

	/* Triggering soft IRQ: stream finish */
	spin_lock_irqsave(&dnx->debug_irq_slck, flags);
	dnx->debug_irq = 0;
	spin_unlock_irqrestore(&dnx->debug_irq_slck, flags);
	
	dnx_reg_write(dnx, DNX_REG_CONTROL_IRQ_TRIGGER, irq);

	wait_event_interruptible(dnx->debug_irq_waitq, dnx->debug_irq);
}


static int test_soft_irq(struct dnx_device *dnx)
{
	int err = 0;

	dev_info(dnx->dev, " Testing IRQs...\n");

	trigger_irq(dnx, DNX_IRQ_MASK_STREAM_DONE);
	if(dnx->debug_irq ^ (DNX_IRQ_MASK_STREAM_DONE | DNX_IRQ_MASK_STREAM_SOFT)) {
		dev_err(dnx->dev, "IRQ was 0x%08x but should be 0x%08x", dnx->debug_irq,
				DNX_IRQ_MASK_STREAM_DONE | DNX_IRQ_MASK_STREAM_SOFT);
		err |= 1;
	}

	trigger_irq(dnx, DNX_IRQ_MASK_SHADER_TRAP);
	if(dnx->debug_irq ^ (DNX_IRQ_MASK_SHADER_TRAP | DNX_IRQ_MASK_STREAM_SOFT)) {
		dev_err(dnx->dev, "IRQ was 0x%08x but should be 0x%08x", dnx->debug_irq,
				DNX_IRQ_MASK_SHADER_TRAP | DNX_IRQ_MASK_STREAM_SOFT);
		err |= 1;

	}

	return err;
}


static int test_reset(struct dnx_device *dnx)
{
	dev_info(dnx->dev, " Testing reset...\n");
	dnx_reg_write(dnx, DNX_REG_CONTROL_SYNC_0, 0xc0ffee42);
	
	if(0xc0ffee42 != dnx_reg_read(dnx, DNX_REG_CONTROL_SYNC_0)) {
		dev_err(dnx->dev, "could not write REG_SYNC0");
		return -1;
	}
	
	dnx_hw_reset(dnx);

	if(dnx_reg_read(dnx, DNX_REG_CONTROL_SYNC_0)) {
		dev_err(dnx->dev, "hw reset failed");
		return -1;
	}

	return 0;
}


int dnx_selftest(struct dnx_device *dnx)
{
	int err = 0;

	err |= test_reset(dnx);
	err |= test_soft_irq(dnx);

	return err;
}
