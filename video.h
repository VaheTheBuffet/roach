#ifndef VIDEO_H
#define VIDEO_H

#ifndef VIDEO_CPP

extern int v_width;

extern int v_height;

extern float v_inv_ar;

extern float v_near;

extern float v_fov;

extern float v_fov_tan;

extern float v_viewport_width;

extern float v_viewport_height;

#else
#define WIDTH 1280
#define HEIGHT 720
#define NEAR 0.1f
#define FOV 90.f
#endif

void write_buf_to_file(unsigned char *buf, int width, int height);

#endif
