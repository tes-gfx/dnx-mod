#include "dnx_dbg.h"

#include "dnx_drv.h"
#include "dnx_gpu.h"
#include "dnx_gem.h"
#include "nx_register_address.h"


#define STR(x) #x
#define EVAL(x,y) x(y)


void dnx_debug_irq(struct dnx_device *dnx, u32 irq_state)
{
	if(irq_state & DNX_IRQ_MASK_SHADER_TRAP) {
		dev_err(dnx->dev, "IRQ error %s\n", STR(DNX_IRQ_MASK_SHADER_TRAP));
	}
	if(irq_state & DNX_IRQ_MASK_SHADER_ILL_OP) {
		dev_err(dnx->dev, "IRQ error %s\n", STR(DNX_IRQ_MASK_SHADER_ILL_OP));
	}
	if(irq_state & DNX_IRQ_MASK_SHADER_RANGE_ERR) {
		dev_err(dnx->dev, "IRQ error %s\n", STR(DNX_IRQ_MASK_SHADER_RANGE_ERR));
	}
	if(irq_state & DNX_IRQ_MASK_SHADER_STACK_OFL) {
		dev_err(dnx->dev, "IRQ error %s\n", STR(DNX_IRQ_MASK_SHADER_STACK_OFL));
	}
	if(irq_state & DNX_IRQ_MASK_SDMA_ALIGN) {
		dev_err(dnx->dev, "IRQ error %s\n", STR(DNX_IRQ_MASK_SDMA_ALIGN));
	}
	if(irq_state & DNX_IRQ_MASK_SDMA_CRC) {
		dev_err(dnx->dev, "IRQ error %s\n", STR(DNX_IRQ_MASK_SDMA_CRC));
	}
	if(irq_state & DNX_IRQ_MASK_STREAM_ERR) {
		dev_err(dnx->dev, "IRQ error %s\n", STR(DNX_IRQ_MASK_STREAM_ERR));
	}
	if(irq_state & DNX_IRQ_MASK_STREAM_RETRIG) {
		dev_err(dnx->dev, "IRQ error %s\n", STR(DNX_IRQ_MASK_STREAM_RETRIG));
	}
	if(irq_state & DNX_IRQ_MASK_REGISTER_ERR) {
		dev_err(dnx->dev, "IRQ error %s\n", STR(DNX_IRQ_MASK_REGISTER_ERR));
	}
	if(irq_state & DNX_IRQ_MASK_JFLAG_OVERRUN) {
		dev_err(dnx->dev, "IRQ error %s\n", STR(DNX_IRQ_MASK_JFLAG_OVERRUN));
	}
}


void dnx_debug_reg_dump(struct dnx_device *dnx)
{
	dev_info(dnx->dev, "Register dump============================================================\n");
	dev_info(dnx->dev, " stream address: 0x%08x\n", dnx_reg_read(dnx, DNX_REG_CONTROL_STREAM_ADDR));
	dev_info(dnx->dev, " stream pos    : 0x%08x\n", dnx_reg_read(dnx, DNX_REG_CONTROL_STREAM_POS));
	dev_info(dnx->dev, " busy vector   : 0x%08x\n", dnx_reg_read(dnx, DNX_REG_CONTROL_BUSY));
	dev_info(dnx->dev, "=========================================================================\n");
}


static struct dnx_bo *find_bo_by_dma_addr(struct dnx_cmdbuf *cmdbuf, dma_addr_t addr)
{
	int i;

	for(i = 0; i < cmdbuf->nr_bos; ++i) {
		size_t size = cmdbuf->bos[i]->base.size;
		dma_addr_t off = addr - cmdbuf->bos[i]->paddr;

		if(off < size) {
			return cmdbuf->bos[i];
		}
	}

	return NULL;
}


/* note: caller must make sure that the device's lock is held when calling
 * this function. */
static struct dnx_cmdbuf *find_cmdbuf_by_dma_addr(struct dnx_device *dnx, dma_addr_t addr, struct dnx_bo **bo)
{
	struct dnx_cmdbuf *cmdbuf = NULL, *tmp;

	if(bo)
		*bo = NULL;

	list_for_each_entry_safe(cmdbuf, tmp, &dnx->active_cmd_list, node) {
		struct dnx_bo *obj = find_bo_by_dma_addr(cmdbuf, addr);

		if(obj) {
			if(bo)
				*bo = obj;
			return cmdbuf;
		}
	}

	return NULL;
}


static void print_buffer_context(struct dnx_device *dnx, void *vaddr, dma_addr_t paddr, size_t size, u32 word_offset)
{
	int start, end, i;
	u32 *buffer = vaddr;

	start = (word_offset < 5) ? -word_offset : -5;
	end = (word_offset > (size / sizeof(*buffer) - 6)) ? (word_offset - size / sizeof(*buffer)) : 6;
	for(i = start; i < end; ++i) {
		dev_info(dnx->dev, "%s0x%08x: %08x\n", i ? " " : ">", paddr + (word_offset + i) * sizeof(*buffer),
				buffer[word_offset + i]);
	}
}


void dnx_debug_stream_err(struct dnx_device *dnx)
{
	u32 stream_pos = dnx_reg_read(dnx, DNX_REG_CONTROL_STREAM_POS);

	dev_info(dnx->dev, "Context =================================================================\n");

	if((stream_pos - dnx->buffer->paddr) < dnx->buffer->size) {
		u32 offset = (stream_pos - dnx->buffer->paddr) / sizeof(u32);

		dev_info(dnx->dev, "Error in ring buffer:\n");

		print_buffer_context(dnx, dnx->buffer->vaddr, dnx->buffer->paddr, dnx->buffer->size, offset);
	}
	else {
		struct dnx_cmdbuf *cmdbuf;
		struct dnx_bo *bo = NULL;

		dev_info(dnx->dev, "Error in user job:\n");

		mutex_lock(&dnx->lock);
		cmdbuf = find_cmdbuf_by_dma_addr(dnx, stream_pos, &bo);
		if(cmdbuf) {
			dev_info(dnx->dev, " cmdbuf_obj=0x%p\n", cmdbuf);
			dev_info(dnx->dev, "  start=0x%pad\n", &cmdbuf->paddr);
			dev_info(dnx->dev, "  nr_bos=0x%u\n", cmdbuf->nr_bos);
			dev_info(dnx->dev, "  fence=0x%u\n", cmdbuf->fence);
			dev_info(dnx->dev, " gem_obj=0x%p\n", bo);
			dev_info(dnx->dev, "  phys=0x%pad\n", &bo->paddr);
			dev_info(dnx->dev, "  size=0x%zx\n", bo->base.size);
			print_buffer_context(dnx, bo->vaddr, bo->paddr, bo->base.size, (stream_pos - cmdbuf->paddr) / sizeof(u32));
		}
		mutex_unlock(&dnx->lock);
	}

	dev_info(dnx->dev, "=========================================================================\n");
}
