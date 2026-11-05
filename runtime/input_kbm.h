/* SwitchRecomp - Copyright 2026 MZ Labs
   Licensed under the Apache License 2.0. See LICENSE.txt and NOTICE.txt;
   any copy or derived work must keep the credit to SwitchRecomp by MZ Labs.
   Shared keyboard and mouse layer: Copyright 2026 Zorkats, same licence. */
/* Keyboard and mouse as a Switch controller, shared by the window backends (backend_win32.c, backend_x11.c).
   A backend only translates its native events: keys to KBM_KEY_* codes, raw mouse movement to counts, mouse buttons
   to KBM_MOUSE_*. This module owns the bindings, the mouse look and the [Input] mouse_* settings.
   Threads: the event functions may run on the window thread; kbm_keyboard / kbm_mouse run on the input thread
   (video_poll). */
#ifndef SWR_INPUT_KBM_H
#define SWR_INPUT_KBM_H
#include "rt.h"

/* key codes: letters 'A'..'Z' and digits '0'..'9' are their ASCII upper-case codes, space is ' ' */
enum {
  KBM_KEY_NONE = 0,
  KBM_KEY_SPACE = ' ',
  KBM_KEY_ENTER = 128,
  KBM_KEY_BACKSPACE,
  KBM_KEY_LEFT,
  KBM_KEY_UP,
  KBM_KEY_RIGHT,
  KBM_KEY_DOWN,
  KBM_KEY_COUNT = 256
};

/* mouse buttons, in the order [Input] mouse_buttons lists them */
enum { KBM_MOUSE_LEFT, KBM_MOUSE_RIGHT, KBM_MOUSE_MIDDLE, KBM_MOUSE_BACK, KBM_MOUSE_FORWARD, KBM_MOUSE_COUNT };

void kbm_key(int key, int down);
void kbm_mouse_motion(int dx, int dy);   /* relative device counts, screen axes (y down) */
void kbm_mouse_button(int button, int down);
void kbm_release_all(void);              /* focus lost, the in-game menu opened */
int kbm_mouse_look_enabled(void);        /* [Input] mouse_look: the backend captures and hides the cursor over the game */

/* the keyboard's buttons (bits 0..15, not remapped) and stick axes; a stick the keyboard leaves at rest stays 0 */
void kbm_keyboard(u64* buttons, i32* lx, i32* ly, i32* rx, i32* ry);
/* the mouse onto the polled state: its buttons, and mouse look on the right stick while the mouse moves.
   active = 0 (the in-game menu is open) drops the movement and the clicks, so nothing jumps when it closes */
void kbm_mouse(u64* buttons, i32* rx, i32* ry, int active);

void mouse_map_reset(void);              /* [Input] mouse_buttons changed (main.c ini_apply) */

#endif
