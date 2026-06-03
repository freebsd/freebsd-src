#ifndef _CAMERA_MOCK_H_
#define _CAMERA_MOCK_H_

#include "camera.h"

#define CAM_MOCK_BARS 0
#define CAM_MOCK_GRADIENT 1

int cam_mock_generate_frame(cam_handle_t *handle, cam_frame_t *frame);
int cam_mock_init(cam_handle_t *handle);
void cam_mock_cleanup(cam_handle_t *handle);

#endif
