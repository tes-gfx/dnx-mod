#include "dnx_buffer.h"

#include "dnx_gpu.h"

#include "nx_types.h"
#include "nx_register_address.h"


static inline void OUT(struct dnx_ringbuf *buffer, u32 data)
{
	u32 *vaddr = (u32*) buffer->vaddr;

	BUG_ON(buffer->user_size >= buffer->size);

	vaddr[buffer->user_size / sizeof(*vaddr)] = data;
	buffer->user_size += sizeof(*vaddr);
}


static inline void CMD_END(struct dnx_ringbuf *buffer)
{
	dnx_stream_cmd_word_t cmd;

	cmd.m_data = 0;
	cmd.bits.m_cmd = DNX_STREAM_CMD_END;

	OUT(buffer, cmd.m_data);
}


static inline void CMD_SYNC(struct dnx_ringbuf *buffer, u32 syncid)
{
	dnx_stream_cmd_word_t cmd;

	cmd.m_data = 0;
	cmd.bits.m_cmd = DNX_STREAM_CMD_WRITE;
	cmd.bits.m_count = 1;
	cmd.bits.m_addr = DNX_REG_CONTROL_SYNC_0;

	OUT(buffer, cmd.m_data);
	OUT(buffer, syncid);
}


void dnx_buffer_init(struct dnx_device *dnx)
{
	struct dnx_ringbuf *buffer = dnx->buffer;

	buffer->user_size = 0;

	CMD_END(buffer);

	/* Spacer for jump address */
	buffer->user_size += 4;
}


static u32 dnx_buffer_reserve(struct dnx_device *dnx,
		struct dnx_ringbuf *buffer, unsigned int cmd_dwords)
{
	if(buffer->user_size + cmd_dwords * sizeof(u32) > buffer->size) {
		dev_dbg(dnx->dev, "buffer wrap around\n");
		buffer->user_size = 0;
	}

	return buffer->paddr + buffer->user_size;
}


static void patch_jmp(struct dnx_device *dnx, void *patch_addr, u32 data)
{
	u32 *word = patch_addr;

	*word = data;
}


void dnx_buffer_queue(struct dnx_device *dnx, struct dnx_cmdbuf *cmdbuf)
{
	dnx_stream_cmd_word_t cmd;
	struct dnx_ringbuf *buffer = dnx->buffer;
	u32 *lw = buffer->vaddr + buffer->user_size - 8; /* position of last end */
	u32 link_target;
	u32 return_target;
	unsigned long flags;

	link_target = cmdbuf->paddr;

	/* we leave space for the cmdbuf's syncid write (2 words) and the end
	 * cmd (1 word) + 1 word for the next jump that will be added with the
	 * next queuing */
	return_target = dnx_buffer_reserve(dnx, buffer, 4);

	patch_jmp(dnx, cmdbuf->vjmpaddr, return_target);

	CMD_SYNC(buffer, cmdbuf->fence);
	CMD_END(buffer);
	buffer->user_size += 4; /* reserve word for jump address */

	/* now change the END into a JMP command, but write the address first */
	lw[1] = cmdbuf->paddr;
	mb();

	cmd.m_data = 0;
	cmd.bits.m_cmd = DNX_STREAM_CMD_JMP;
	cmd.bits.m_count = 1;
	lw[0] = cmd.m_data;
	mb();

	/* If the STC has already halted, we can start in the job. If it
	 * is still running, it will see the inserted jump. */
	spin_lock_irqsave(&dnx->stc_lock, flags);
	dnx->fence_active = cmdbuf->fence;
	if(!dnx->stc_running & (dnx->fence_completed != cmdbuf->fence)) {
		dnx->stc_running = true;
		dnx_reg_write(dnx, DNX_REG_CONTROL_STREAM_ADDR, cmdbuf->paddr);
	}
	spin_unlock_irqrestore(&dnx->stc_lock, flags);
}
