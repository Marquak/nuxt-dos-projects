#ifndef ORBIT_H
#define ORBIT_H
/* All interfaces are 8086 small-model DOS. Coordinates are signed 16-bit. */
typedef struct { int x,y,z; } Vec3;
typedef struct { int x,y; unsigned char valid; } Vec2;

int video_open(int resolution); /* 0=640x480,1=800x600,2=1024x768, 16 colours */
void video_close(void);
int video_width(void);
int video_height(void);
void video_clear(void);
void video_line(int x0,int y0,int x1,int y1,unsigned color);
void video_pixel(int x,int y,unsigned color);
void video_text(int x,int y,unsigned color,const char *s,int scale);
void video_palette(const unsigned char *rgb); /* 16 triples, values 0..63 */
void video_reference(int enabled); /* test/benchmark: C reference rasterizer */
unsigned long video_checksum(void);
const char *video_driver(void);
int video_buffering(int enabled); /* probe two independent screen pages */
void video_select_page(unsigned page); /* draw/read target, never scanout */
void video_show_page(unsigned page); /* completed page, at retrace if possible */
const char *video_buffer_name(void);

int math_detect(void);
void math_init(int use87);
void math_end(void);
int math_uses87(void);
void math_matrix(unsigned yaw,unsigned pitch,unsigned roll); /* 256/turn */
void math_project(const Vec3 *in,Vec2 *out,unsigned count,int scale,int distance);
void math_gravity_reset(void);
void math_gravity_step(Vec3 *out,unsigned count); /* at most 48 particles */
void math_attractor_reset(void);
void math_attractor_step(Vec3 *out,unsigned count); /* sequential trail points */
int math_sin(unsigned phase); /* fixed-point sine, amplitude 256 */

int music_start(int silent); /* always installs clock; returns AdLib present */
void music_stop(void);
unsigned long music_ticks(void); /* monotonic 18.2Hz time, atomic snapshot */
void music_pause(int paused);
void music_mute(int muted);
void music_section(unsigned section); /* scene tag; never restarts the score */
void music_restart(void); /* restart whole score, preserve clock */
int music_finished(void);
unsigned music_pulse(void); /* 0..12 kick envelope for visual accents */
unsigned music_beat(void); /* sequencer step for visual accents */
unsigned music_notes(void);
int music_present(void);
#endif
