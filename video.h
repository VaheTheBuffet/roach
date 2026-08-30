#ifndef VIDEO_H
#define VIDEO_H

#ifndef VIDEO_CPP
// these are exported to other files

extern int v_width;
extern int v_height;
extern float v_inv_ar;
extern float v_near;
extern float v_fov;
extern float v_fov_tan;
extern float v_viewport_width;
extern float v_viewport_height;

#else
// these are internal
#define WIDTH 1280
#define HEIGHT 720
#define NEAR 0.1f
#define FOV 90.f
#endif

void write_buf_to_file(unsigned char *buf, int width, int height);

// using these function will also update all dependent variables
// TODO: maybe abstract all the variables and make them private
void v_set_w_h(int w, int h);
void v_set_fov(float f);

#endif
