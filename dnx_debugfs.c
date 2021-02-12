#include "dnx_debugfs.h"

#include "dnx_drv.h"
#include "dnx_gpu.h"

#include "nx_register_address.h"


#define STRING(x) #x


static void print_reg(struct seq_file *m, const char *name, u32 val)
{
	seq_printf(m, "0x%08x\t%s\n", val, name);
}


static int show_gpu_regs(struct dnx_device *dnx, struct seq_file *m)
{
	u32 *reg = (u32*) dnx->mmio;
	int i=0;

	print_reg(m, STRING(DNX_REG_CONTROL_VERSION       ), reg[i++]);
	print_reg(m, STRING(DNX_REG_CONTROL_CONFIG_1      ), reg[i++]);
	print_reg(m, STRING(DNX_REG_CONTROL_CONFIG_2      ), reg[i++]);
	print_reg(m, STRING(DNX_REG_CONTROL_CONFIG_3      ), reg[i++]);
	print_reg(m, STRING(DNX_REG_CONTROL_BUSY          ), reg[i++]);
	print_reg(m, STRING(DNX_REG_CONTROL_IRQ_MASK      ), reg[i++]);
	print_reg(m, STRING(DNX_REG_CONTROL_IRQ_STATE     ), reg[i++]);
	print_reg(m, STRING(DNX_REG_CONTROL_STREAM_ADDR   ), reg[i++]);
	print_reg(m, STRING(DNX_REG_CONTROL_STREAM_POS    ), reg[i++]);
	print_reg(m, STRING(DNX_REG_CONTROL_SYNC_0        ), reg[i++]);
	print_reg(m, STRING(DNX_REG_CONTROL_SYNC_1        ), reg[i++]);
	print_reg(m, STRING(DNX_REG_CONTROL_SYNC_2        ), reg[i++]);
	print_reg(m, STRING(DNX_REG_CONTROL_RETURN_ADDRESS), reg[i++]);

	return 0;
}


static void print_buffer(struct dnx_device *dnx, struct seq_file *m)
{
	struct dnx_ringbuf *buf = dnx->buffer;
	u32 size = buf->size;
	u32 *ptr = buf->vaddr;
	u32 i;

	seq_printf(m, "virt %p - phys 0x%llx - free 0x%08x\n",
			buf->vaddr, (u64)buf->paddr, size - buf->user_size);

	for (i = 0; i < size / 4; i++) {
		if (i && !(i % 4))
			seq_puts(m, "\n");
		if (i % 4 == 0)
			seq_printf(m, "\t0x%p: ", ptr + i);
		seq_printf(m, "%08x ", *(ptr + i));
	}
	seq_puts(m, "\n");
}


static int show_ring(struct dnx_device *dnx, struct seq_file *m)
{
	seq_printf(m, "Ring buffer (%s): ", dev_name(dnx->dev));

	mutex_lock(&dnx->lock);
	print_buffer(dnx, m);
	mutex_unlock(&dnx->lock);

	return 0;
}


static int show_mm(struct dnx_device *dnx, struct seq_file *m)
{
	struct drm_info_node *node = (struct drm_info_node *) m->private;
	struct drm_device *dev = node->minor->dev;
	struct drm_printer p = drm_seq_file_printer(m);
	
	drm_mm_print(&dev->vma_offset_manager->vm_addr_space_mm, &p);

	return 0;
}


static int show_busy(struct dnx_device *dnx, struct seq_file *m)
{
	u32 *reg = (u32*) dnx->mmio;
	u32 busy;
	int i;

	busy = reg[DNX_REG_CONTROL_BUSY];

	if(!busy) {
		seq_printf(m, "core ready...\n");
	}
	else {
		seq_printf(m, "core busy (0x%0x):\n", busy);
		if(busy & DNX_BUSY_MASK_CTRL)
			seq_printf(m, STRING(DNX_BUSY_MASK_CTRL) "\n");
		if(busy & DNX_BUSY_MASK_REG)
			seq_printf(m, STRING(DNX_BUSY_MASK_REG) "\n");
		if(busy & DNX_BUSY_MASK_SDMA)
			seq_printf(m, STRING(DNX_BUSY_MASK_SDMA) "\n");
		if(busy & DNX_BUSY_MASK_DRAW)
			seq_printf(m, STRING(DNX_BUSY_MASK_DRAW) "\n");
		if(busy & DNX_BUSY_MASK_RAST)
			seq_printf(m, STRING(DNX_BUSY_MASK_RAST) "\n");
		if(busy & DNX_BUSY_MASK_DISP)
			seq_printf(m, STRING(DNX_BUSY_MASK_DISP) "\n");
		if(busy & DNX_BUSY_MASK_PASM)
			seq_printf(m, STRING(DNX_BUSY_MASK_PASM) "\n");
		if(busy & DNX_BUSY_MASK_SCR)
			seq_printf(m, STRING(DNX_BUSY_MASK_SCR) "\n");
		if(busy & DNX_BUSY_MASK_FCLR)
			seq_printf(m, STRING(DNX_BUSY_MASK_FCLR) "\n");
		if(busy & DNX_BUSY_MASK_L2C)
			seq_printf(m, STRING(DNX_BUSY_MASK_L2C) "\n");
		if(busy & DNX_BUSY_MASK_BLU)
			seq_printf(m, STRING(DNX_BUSY_MASK_BLU) "\n");
		for(i=0; i<4; ++i) {
			if(busy & (DNX_BUSY_MASK_TFUBASE << i))
				seq_printf(m, "DNX_BUSY_MASK_TFU%d\n", i);
		}
		for(i=0; i<4; ++i) {
			if(busy & (DNX_BUSY_MASK_SHDBASE << i))
				seq_printf(m, "DNX_BUSY_MASK_SHD%d\n", i);
		}
	}

	return 0;
}


static int show_unlocked(struct seq_file *m, void *arg)
{
	struct drm_info_node *node = (struct drm_info_node *) m->private;
	struct drm_device *dev = node->minor->dev;
	struct dnx_device *dnx = dev->dev_private;
	int (*show)(struct dnx_device *dnx, struct seq_file *m) =
			node->info_ent->data;

	return show(dnx, m);
}


static int show_status(struct dnx_device *dnx, struct seq_file *m)
{
	u32 *reg = (u32*) dnx->mmio;

	seq_printf(m, "STC: ");
	if(dnx->stc_running) {
		seq_printf(m, "running (0x%08x)\n", reg[DNX_REG_CONTROL_STREAM_POS]);
	}
	else {
		seq_printf(m, "stopped (0x%08x)\n", reg[DNX_REG_CONTROL_STREAM_POS]);
	}

	seq_printf(m, "last fence: %d\n", reg[DNX_REG_CONTROL_SYNC_0]);

	return 0;
}


/* todo: replace by writable sysfs file and reset/recover when 1 is written to it */
static int show_reset(struct dnx_device *dnx, struct seq_file *m)
{
	seq_printf(m, "resetting core...\n");

	dnx_gpu_recover_hangup(dnx);

	return 0;
}


static struct drm_info_list dnx_debugfs_list[] = {
		{"gpu", show_unlocked, 0, show_gpu_regs},
		{"ring", show_unlocked, 0, show_ring},
		{"mm", show_unlocked, 0, show_mm},
		{"busy", show_unlocked, 0, show_busy},
		{"reset", show_unlocked, 0, show_reset},
		{"status", show_unlocked, 0, show_status},
};


int dnx_debugfs_init(struct drm_minor *minor)
{
	struct drm_device *dev = minor->dev;
	int ret;

	ret = drm_debugfs_create_files(dnx_debugfs_list,
			ARRAY_SIZE(dnx_debugfs_list),
			minor->debugfs_root, minor);

	if (ret) {
		dev_err(dev->dev, "could not install dnx_debugfs_list\n");
		return ret;
	}

	return ret;
}


void dnx_debugfs_cleanup(struct drm_minor *minor)
{
	drm_debugfs_remove_files(dnx_debugfs_list,
			ARRAY_SIZE(dnx_debugfs_list), minor);
}
