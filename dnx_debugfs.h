#ifndef _DNX_DEBUGFS_H_
#define _DNX_DEBUGFS_H_
#ifdef CONFIG_DEBUG_FS


#include <drm/drmP.h>


int dnx_debugfs_init(struct drm_minor *minor);
void dnx_debugfs_cleanup(struct drm_minor *minor);


#endif /* CONFIG_DEBUG_FS */
#endif /* _DNX_DEBUGFS_H_ */
