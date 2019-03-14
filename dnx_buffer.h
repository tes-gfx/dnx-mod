#ifndef _DNX_BUFFER_H_
#define _DNX_BUFFER_H_


#include "dnx_gpu.h"


void dnx_buffer_queue(struct dnx_device *dnx, struct dnx_cmdbuf *cmdbuf);
void dnx_buffer_init(struct dnx_device *dnx);


#endif /* _DNX_BUFFER_H_ */
