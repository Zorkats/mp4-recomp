/* runtime/input_kbm.c on its own: the keyboard table, quick taps, mouse look on the right stick and the mouse buttons.
   Run: bash linux/input_kbm_test.sh (prints a FAIL line per mismatch and exits 1). Builds on Windows too:
   gcc -I runtime linux/input_kbm_test.c runtime/input_kbm.c */
#include <stdarg.h>
#include "input_kbm.h"

int g_frames, g_verbose;
int g_opt_mouse_look=1, g_opt_mouse_sens=100, g_opt_mouse_invert_y=0, g_opt_mouse_deadzone=15;
char g_opt_mouse_buttons[64]="zr,zl,r,-,-";
void logf_(int l, const char* f, ...){ (void)l; (void)f; }

static int failed;
static void want_u(const char* what, u64 got, u64 want){
  if(got!=want){ printf("FAIL: %s = %#llx (want %#llx)\n",what,(unsigned long long)got,(unsigned long long)want); failed=1; }
  else printf("ok: %s\n",what); }
static void want_i(const char* what, i32 got, i32 want){
  if(got!=want){ printf("FAIL: %s = %d (want %d)\n",what,got,want); failed=1; }
  else printf("ok: %s\n",what); }
static void want_range(const char* what, i32 got, i32 lo, i32 hi){
  if(got<lo || got>hi){ printf("FAIL: %s = %d (want %d..%d)\n",what,got,lo,hi); failed=1; }
  else printf("ok: %s\n",what); }

typedef struct { u64 b; i32 lx, ly, rx, ry; } Poll;
static Poll keyboard(void){ Poll p; kbm_keyboard(&p.b,&p.lx,&p.ly,&p.rx,&p.ry); return p; }
/* a poll with the controller's right stick at (rx, ry) */
static Poll mouse(i32 rx, i32 ry, int active){ Poll p={0,0,0,rx,ry}; kbm_mouse(&p.b,&p.rx,&p.ry,active); return p; }

int main(void){
  /* keyboard */
  kbm_key('X',1); want_u("X is A",keyboard().b,1ull<<0);
  kbm_key(KBM_KEY_SPACE,1); kbm_key(KBM_KEY_SPACE,0); keyboard();
  want_u("letting go of Space keeps A while X is held",keyboard().b,1ull<<0);
  kbm_key('X',0); want_u("X released",keyboard().b,0);
  kbm_key('Z',1); kbm_key('Z',0);
  want_u("a tap between two polls reaches one poll",keyboard().b,1ull<<1);
  want_u("and only one",keyboard().b,0);
  kbm_key('W',1); kbm_key('A',1); kbm_key('D',1);
  { Poll p=keyboard(); want_i("W pushes the left stick up",p.ly,32767); want_i("A and D cancel",p.lx,0); want_i("no right stick",p.rx,0); }
  kbm_key('L',1); want_i("L pushes the right stick right",keyboard().rx,32767);
  kbm_key('1',1); kbm_key('3',1); kbm_key(KBM_KEY_ENTER,1);
  want_u("1 ZL, 3 ZR, Enter plus",keyboard().b,(1ull<<8)|(1ull<<9)|(1ull<<10));
  kbm_release_all(); { Poll p=keyboard(); want_u("released all: no buttons",p.b,0); want_i("released all: no stick",p.ly,0); }
  kbm_key(-5,1); kbm_key(KBM_KEY_COUNT,1); want_u("out-of-range keys ignored",keyboard().b,0);

  /* mouse look */
  want_i("a still mouse leaves the controller's right stick",mouse(1234,-99,1).rx,1234);
  kbm_mouse_motion(40,0); { Poll p=mouse(0,0,1); want_i("40 counts at 100% is a full push right",p.rx,32767); want_i("no vertical movement, no vertical push",p.ry,0); }
  kbm_mouse_motion(4,0); want_range("4 counts: the dead zone push plus a little",mouse(0,0,1).rx,7600,7800);   /* (0.15+0.1*0.85)*32767 */
  kbm_mouse_motion(-1,0); want_range("1 count left still turns: at least the dead zone push",mouse(0,0,1).rx,-5800,-4915);
  kbm_mouse_motion(0,-10); want_range("the mouse moving up looks up",mouse(0,0,1).ry,1,32767);
  g_opt_mouse_invert_y=1; kbm_mouse_motion(0,-10); want_range("inverted: up looks down",mouse(0,0,1).ry,-32767,-1); g_opt_mouse_invert_y=0;
  kbm_mouse_motion(1000,1000); { Poll p=mouse(0,0,1); want_i("a huge movement is clamped (x)",p.rx,32767); want_i("a huge movement is clamped (y)",p.ry,-32767); }
  g_opt_mouse_sens=200; kbm_mouse_motion(20,0); want_i("200% sensitivity: half the movement for a full push",mouse(0,0,1).rx,32767); g_opt_mouse_sens=100;
  kbm_mouse_motion(40,0); want_i("menu open: the movement is dropped",mouse(0,0,0).rx,0);
  want_i("and doesn't arrive once it closes",mouse(0,0,1).rx,0);
  g_opt_mouse_look=0; kbm_mouse_motion(40,0); want_i("mouse_look=0: no camera",mouse(0,0,1).rx,0); g_opt_mouse_look=1;

  /* mouse buttons */
  kbm_mouse_button(KBM_MOUSE_LEFT,1); want_u("left click is ZR",mouse(0,0,1).b,1ull<<9);
  kbm_mouse_button(KBM_MOUSE_LEFT,0); want_u("released",mouse(0,0,1).b,0);
  kbm_mouse_button(KBM_MOUSE_RIGHT,1); kbm_mouse_button(KBM_MOUSE_MIDDLE,1); want_u("right ZL, middle R",mouse(0,0,1).b,(1ull<<8)|(1ull<<7));
  kbm_mouse_button(KBM_MOUSE_RIGHT,0); kbm_mouse_button(KBM_MOUSE_MIDDLE,0); mouse(0,0,1);
  kbm_mouse_button(KBM_MOUSE_LEFT,1); kbm_mouse_button(KBM_MOUSE_LEFT,0); want_u("a click between two polls reaches one poll",mouse(0,0,1).b,1ull<<9);
  want_u("and only one",mouse(0,0,1).b,0);
  kbm_mouse_button(KBM_MOUSE_BACK,1); want_u("back is unbound by default",mouse(0,0,1).b,0); kbm_mouse_button(KBM_MOUSE_BACK,0);
  kbm_mouse_button(KBM_MOUSE_LEFT,1); want_u("menu open: no clicks",mouse(0,0,0).b,0); kbm_mouse_button(KBM_MOUSE_LEFT,0); mouse(0,0,1);
  snprintf(g_opt_mouse_buttons,sizeof g_opt_mouse_buttons," A , minus ,nonsense,y,b"); mouse_map_reset();
  kbm_mouse_button(KBM_MOUSE_LEFT,1); kbm_mouse_button(KBM_MOUSE_RIGHT,1); kbm_mouse_button(KBM_MOUSE_MIDDLE,1);
  kbm_mouse_button(KBM_MOUSE_BACK,1); kbm_mouse_button(KBM_MOUSE_FORWARD,1);
  want_u("mouse_buttons with spaces, upper case and an unknown name",mouse(0,0,1).b,(1ull<<0)|(1ull<<11)|(1ull<<3)|(1ull<<1));
  kbm_release_all(); want_u("released all: no mouse buttons",mouse(0,0,1).b,0);

  printf("input_kbm test: %s\n",failed? "FAILED" : "all passed");
  return failed;
}
