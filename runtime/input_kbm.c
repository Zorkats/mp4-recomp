/* SwitchRecomp - Copyright 2026 MZ Labs
   Licensed under the Apache License 2.0. See LICENSE.txt and NOTICE.txt;
   any copy or derived work must keep the credit to SwitchRecomp by MZ Labs.
   Shared keyboard and mouse layer: Copyright 2026 Zorkats, same licence. */
/* Keyboard and mouse as a Switch controller (input_kbm.h). The bindings and the mouse button parsing used to live in
   backend_win32.c; the X11 backend had its own copy of the keyboard table and no game mouse. */
#include "input_kbm.h"
#include <ctype.h>
#include <math.h>

extern int g_opt_mouse_look, g_opt_mouse_sens, g_opt_mouse_invert_y, g_opt_mouse_deadzone;   /* main.c [Input] */
extern char g_opt_mouse_buttons[];

/* ---------------------------------------------------------------- keyboard */
/* Switch button bits 0..15; 40..47 are stick directions, turned into axes in kbm_keyboard */
enum { STICK_L_LEFT = 40, STICK_L_UP, STICK_L_RIGHT, STICK_L_DOWN, STICK_R_LEFT, STICK_R_UP, STICK_R_RIGHT, STICK_R_DOWN };

static const struct { int key, bit; } KEYMAP[] = {
  {'X', 0}, {KBM_KEY_SPACE, 0},   /* A */
  {'Z', 1},                       /* B */
  {'C', 2},                       /* X */
  {'V', 3},                       /* Y */
  {'Q', 6},                       /* L */
  {'E', 7},                       /* R */
  {'1', 8},                       /* ZL */
  {'3', 9},                       /* ZR */
  {KBM_KEY_ENTER, 10},            /* + */
  {KBM_KEY_BACKSPACE, 11},        /* - */
  {KBM_KEY_LEFT, 12}, {KBM_KEY_UP, 13}, {KBM_KEY_RIGHT, 14}, {KBM_KEY_DOWN, 15},
  {'A', STICK_L_LEFT}, {'W', STICK_L_UP}, {'D', STICK_L_RIGHT}, {'S', STICK_L_DOWN},
  {'J', STICK_R_LEFT}, {'I', STICK_R_UP}, {'L', STICK_R_RIGHT}, {'K', STICK_R_DOWN},
};

/* per key, not per Switch button: X and Space both press A, and letting go of one must not release the other.
   tapped keeps a press that came and went between two polls, so a quick tap still reaches the game */
static volatile unsigned char g_key_held[KBM_KEY_COUNT], g_key_tapped[KBM_KEY_COUNT];

void kbm_key(int key, int down){
  if(key <= KBM_KEY_NONE || key >= KBM_KEY_COUNT) return;
  g_key_held[key] = down ? 1 : 0;
  if(down) g_key_tapped[key] = 1;
}

static i32 keys_axis(u64 dirs, int neg, int pos){
  return ((dirs >> pos) & 1 ? 32767 : 0) - ((dirs >> neg) & 1 ? 32767 : 0);
}

void kbm_keyboard(u64* buttons, i32* lx, i32* ly, i32* rx, i32* ry){
  u64 bits = 0;
  for(size_t i = 0; i < sizeof KEYMAP / sizeof *KEYMAP; i++){
    int key = KEYMAP[i].key;
    if(g_key_held[key] || g_key_tapped[key]) bits |= 1ull << KEYMAP[i].bit;
  }
  for(size_t i = 0; i < sizeof KEYMAP / sizeof *KEYMAP; i++) g_key_tapped[KEYMAP[i].key] = 0;
  *buttons = bits & 0xFFFFull;
  *lx = keys_axis(bits, STICK_L_LEFT, STICK_L_RIGHT);
  *ly = keys_axis(bits, STICK_L_DOWN, STICK_L_UP);
  *rx = keys_axis(bits, STICK_R_LEFT, STICK_R_RIGHT);
  *ry = keys_axis(bits, STICK_R_DOWN, STICK_R_UP);
}

/* ---------------------------------------------------------------- mouse */
/* movement since the last poll and the buttons held (bit per KBM_MOUSE_*); any thread adds, the input thread takes */
static volatile int g_mouse_dx, g_mouse_dy, g_mouse_held, g_mouse_tapped;

void kbm_mouse_motion(int dx, int dy){
  __atomic_fetch_add(&g_mouse_dx, dx, __ATOMIC_RELAXED);
  __atomic_fetch_add(&g_mouse_dy, dy, __ATOMIC_RELAXED);
}

void kbm_mouse_button(int button, int down){
  if(button < 0 || button >= KBM_MOUSE_COUNT) return;
  int bit = 1 << button;
  if(down){
    __atomic_fetch_or(&g_mouse_held, bit, __ATOMIC_RELAXED);
    __atomic_fetch_or(&g_mouse_tapped, bit, __ATOMIC_RELAXED);
  } else {
    __atomic_fetch_and(&g_mouse_held, ~bit, __ATOMIC_RELAXED);
  }
}

void kbm_release_all(void){
  for(int i = 0; i < KBM_KEY_COUNT; i++){
    g_key_held[i] = 0;
    g_key_tapped[i] = 0;
  }
  __atomic_store_n(&g_mouse_held, 0, __ATOMIC_RELAXED);
  __atomic_store_n(&g_mouse_tapped, 0, __ATOMIC_RELAXED);
}

int kbm_mouse_look_enabled(void){ return g_opt_mouse_look; }

/* [Input] mouse_buttons: "left,right,middle,back,forward" as Switch buttons, - for none */
static int g_mouse_bit[KBM_MOUSE_COUNT] = {-2, -2, -2, -2, -2};   /* -2 not parsed yet, -1 none */
void mouse_map_reset(void){ g_mouse_bit[0] = -2; }
static void mouse_map(void){
  if(g_mouse_bit[0] != -2) return;
  static const struct { const char* name; int bit; } NAMES[] = {
    {"a", 0}, {"b", 1}, {"x", 2}, {"y", 3}, {"lstick", 4}, {"rstick", 5}, {"l", 6}, {"r", 7},
    {"zl", 8}, {"zr", 9}, {"plus", 10}, {"minus", 11},
  };
  const char* p = g_opt_mouse_buttons;
  for(int i = 0; i < KBM_MOUSE_COUNT; i++){
    char name[16];
    int n = 0;
    while(*p == ' ') p++;
    while(*p && *p != ',' && n < 15) name[n++] = (char)tolower((unsigned char)*p++);
    while(n > 0 && name[n - 1] == ' ') n--;
    name[n] = 0;
    while(*p && *p != ',') p++;
    if(*p == ',') p++;
    g_mouse_bit[i] = -1;
    for(size_t j = 0; j < sizeof NAMES / sizeof *NAMES; j++)
      if(!strcmp(name, NAMES[j].name)) g_mouse_bit[i] = NAMES[j].bit;
  }
}

/* test only: SWR_TEST_MOUSE="FRAME:DX,DY,BUTTONS;..." adds that movement at that guest frame and holds those buttons
   (hex, 1 left 2 right 4 middle) from there to the next entry */
static void mouse_test(int* dx, int* dy, int* held){
  static const char* s;
  static int init, test_held;
  if(!init){
    init = 1;
    s = getenv("SWR_TEST_MOUSE");
  }
  if(!s) return;
  while(*s){
    char* e;
    long frame = strtol(s, &e, 10);
    if(e == s || *e != ':' || g_frames < frame) break;
    long x = strtol(e + 1, &e, 10);
    if(*e == ',') e++;
    long y = strtol(e, &e, 10);
    if(*e == ',') e++;
    long b = strtol(e, &e, 16);
    *dx += (int)x;
    *dy += (int)y;
    test_held = (int)b;
    s = *e == ';' ? e + 1 : e;
  }
  *held |= test_held;
}

/* Mouse look. The game turns the camera at a speed set by how far the right stick is pushed, so the mouse's speed
   (counts per poll, 60 a second) becomes a stick deflection: KBM_LOOK_FULL_COUNTS counts in one poll at 100%
   sensitivity is a full push. The game ignores small pushes (its stick dead zone), so any movement starts at
   [Input] mouse_deadzone percent, and slow mouse movement still turns the camera slowly. */
#define KBM_LOOK_FULL_COUNTS 40.0f
static float look_axis(int counts, float gain, float dead){
  if(counts == 0) return 0.0f;
  float push = dead + fabsf((float)counts) * gain * (1.0f - dead);
  if(push > 1.0f) push = 1.0f;
  return counts < 0 ? -push : push;
}

void kbm_mouse(u64* buttons, i32* rx, i32* ry, int active){
  int dx = __atomic_exchange_n(&g_mouse_dx, 0, __ATOMIC_RELAXED);
  int dy = __atomic_exchange_n(&g_mouse_dy, 0, __ATOMIC_RELAXED);
  int held = __atomic_load_n(&g_mouse_held, __ATOMIC_RELAXED) | __atomic_exchange_n(&g_mouse_tapped, 0, __ATOMIC_RELAXED);
  mouse_test(&dx, &dy, &held);
  if(!active || !g_opt_mouse_look) return;
  mouse_map();
  for(int i = 0; i < KBM_MOUSE_COUNT; i++)
    if((held >> i & 1) && g_mouse_bit[i] >= 0) *buttons |= 1ull << g_mouse_bit[i];
  if(!dx && !dy) return;   /* the mouse is still: the controller's right stick, if any, stays in charge */
  float gain = (float)g_opt_mouse_sens / 100.0f / KBM_LOOK_FULL_COUNTS;
  float dead = (float)g_opt_mouse_deadzone / 100.0f;
  float sx = look_axis(dx, gain, dead);
  float sy = look_axis(-dy, gain, dead);   /* screen y is down, the stick's up */
  if(g_opt_mouse_invert_y) sy = -sy;
  *rx = (i32)(sx * 32767.0f);
  *ry = (i32)(sy * 32767.0f);
  { static int said; if(!said){ said = 1; LOG("input: mouse look in use\n"); } }
}
