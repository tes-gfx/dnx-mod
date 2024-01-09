#ifndef __DNX_DRV_H__
#define __DNX_DRV_H__

#include <linux/kernel.h>
#include <drm/drmP.h>
#include "dnx_drm.h"

#include "nx_types.h"


#define DNX_IRQ_MASK_ERRORS (\
	DNX_IRQ_MASK_SHADER_TRAP      | \
	DNX_IRQ_MASK_SHADER_ILL_OP    | \
	DNX_IRQ_MASK_SHADER_RANGE_ERR | \
	DNX_IRQ_MASK_SHADER_STACK_OFL | \
	DNX_IRQ_MASK_SDMA_ALIGN       | \
	DNX_IRQ_MASK_SDMA_CRC         | \
	DNX_IRQ_MASK_STREAM_ERR       | \
	DNX_IRQ_MASK_STREAM_RETRIG    | \
	DNX_IRQ_MASK_REGISTER_ERR     | \
	DNX_IRQ_MASK_JFLAG_OVERRUN      \
) // error IRQs (for simple error test in IRQ handler)


struct dnx_device;


u32 dnx_reg_read(struct dnx_device *dnx, u32 reg);
void dnx_reg_write(struct dnx_device *dnx, u32 reg, u32 val);

int dnx_ioctl_gem_submit(struct drm_device *dev, void *data,
		struct drm_file *file);

/*
 * Return the storage size of a structure with a variable length array.
 * The array is nelem elements of elem_size, where the base structure
 * is defined by base.  If the size overflows size_t, return zero.
 */
static inline size_t size_vstruct(size_t nelem, size_t elem_size, size_t base)
{
	if (elem_size && nelem > (SIZE_MAX - base) / elem_size)
		return 0;
	return base + nelem * elem_size;
}

/* returns true if fence a comes after fence b */
static inline bool fence_after(u32 a, u32 b)
{
	return (s32)(a - b) > 0;
}

static inline bool fence_after_eq(u32 a, u32 b)
{
	return (s32)(a - b) >= 0;
}

static inline unsigned long dnx_timeout_to_jiffies(
			const struct timespec *timeout)
{
	unsigned long timeout_jiffies = timespec_to_jiffies(timeout);
	unsigned long start_jiffies = jiffies - INITIAL_JIFFIES;
	unsigned long remaining_jiffies;

	if (time_after(start_jiffies, timeout_jiffies))
		remaining_jiffies = 0;
	else
		remaining_jiffies = timeout_jiffies - start_jiffies;

	return remaining_jiffies;
}

#endif
