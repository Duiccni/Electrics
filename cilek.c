// C:\"Program Files"\LLVM\bin\clang.exe cilek.c -o cilek.exe -O3 -Iinclude -Llib -lSDL3 -Wno-parentheses

#include "SDL3/SDL.h"
#include <stdlib.h>
#include <stdio.h>

float mul2pow(float x, int n) {
   int nx = *(int*)&x, y = nx + (n << 23);
   if ((y ^ nx) & (1 << 31)) return 0.0f; // Check overflow
   return *(float*)&y;
}

#define SCREEN_SIZE 640
#define HALF_SCREEN_SIZE (SCREEN_SIZE >> 1)
#define HSS HALF_SCREEN_SIZE

int log2_dt = -10;

void set_pixel(SDL_Surface *surface, int x, int y, int color) {
   if ((unsigned)x >= surface->w || (unsigned)y >= surface->h) return;
   ((int*)surface->pixels)[surface->w * y + x] = color;
}

void fill_surface(SDL_Surface *surface, int color) {
   int i = surface->w * surface->h;
   while (i) ((int*)surface->pixels)[--i] = color;
}

#define get_sign(x) (((x) >> 31) | 1)

void drawhor(SDL_Surface *surface, int x0, int x1, int y, int color) {
   if (x1 < x0) {
      int t = x0;
      x0 = x1;
      x1 = t;
   }
loop:
   set_pixel(surface, x0, y, color);
   if (x0 == x1) return;
   x0++;
   goto loop;
}

void drawver(SDL_Surface *surface, int y0, int y1, int x, int color) {
   if (y1 < y0) {
      int t = y0;
      y0 = y1;
      y1 = t;
   }
loop:
   set_pixel(surface, x, y0, color);
   if (y0 == y1) return;
   y0++;
   goto loop;
}

void rectangle(SDL_Surface *surface, int x0, int y0, int x1, int y1, int color, bool fill) {
   if (x1 < x0) {
      int t = x0;
      x0 = x1;
      x1 = t;
   }
   if (y1 < y0) {
      int t = y0;
      y0 = y1;
      y1 = t;
   }

   if (fill) {
   loop:
      drawhor(surface, x0, x1, y0, color);
      if (y0 == y1) return;
      y0++;
      goto loop;
   }

   drawhor(surface, x0, x1, y0, color);
   drawhor(surface, x0, x1, y1, color);
   drawver(surface, y0, y1, x0, color);
   drawver(surface, y0, y1, x1, color);
}

void drawline(SDL_Surface *surface, int x0, int y0, int x1, int y1, int color, int thick) {
   int half_thick = thick >> 1;
   thick--;
   bool swap;
   int dx = x1 - x0, dy = y1 - y0;
   if (dx == 0) {
      x0 -= half_thick;
      rectangle(surface, x0, y0, x0 + thick, y1, color, 1);
      return;
   }
   if (swap = (dx < dy)) {
      int t = dy;
      dy = dx;
      dx = t;
      t = y0;
      y0 = x0;
      x0 = t;
   }
   int cof = (dy << 8) / dx;

   int xo = 0, step = get_sign(dx);
   if (thick > 1) {
      while (1) {
         int x = x0 + xo, y = y0 + ((xo * cof) >> 8) - half_thick;
         if (swap) drawhor(surface, y, y + thick, x, color);
         else drawver(surface, y, y + thick, x, color);
         if (xo == dx) return;
         xo += step;
      }

      return;
   }

   while (1) {
      int x = x0 + xo, y = y0 + ((xo * cof) >> 8);
      if (swap) set_pixel(surface, y, x, color);
      else set_pixel(surface, x, y, color);
      if (xo == dx) return;
      xo += step;
   }
}

void drawdot(SDL_Surface *surface, int x, int y, int color) {
   set_pixel(surface, x, y, color);
   set_pixel(surface, x+1, y+1, color);
   set_pixel(surface, x+1, y-1, color);
   set_pixel(surface, x-1, y+1, color);
   set_pixel(surface, x-1, y-1, color);
}

void drawcircle(SDL_Surface *surface, int cx, int cy, int r, int color) {
   int x = 0, y = r;
   r = (r * r) << 2;
   while (x <= y) {
      set_pixel(surface, cx+x, cy+y, color);
      set_pixel(surface, cx+x, cy-y, color);
      set_pixel(surface, cx-x, cy+y, color);
      set_pixel(surface, cx-x, cy-y, color);
      set_pixel(surface, cx+y, cy+x, color);
      set_pixel(surface, cx+y, cy-x, color);
      set_pixel(surface, cx-y, cy+x, color);
      set_pixel(surface, cx-y, cy-x, color);

      x++;
      int mid = (y << 1) - 1;
      if (((x*x) << 2) + mid*mid > r)
         y--;
   }
}

typedef struct {
   int x, y;
} Vec2;

#define NnMAX 512
#define NwMAX 512

int Nn = 0, Nw = 0, Ns = 0;

Vec2 nodes[NnMAX];
float charges[NnMAX];
float mass[NnMAX];

float inv_mass[NnMAX];

Vec2 wires[NwMAX];
float inv_resistances[NwMAX];

float inv_length[NwMAX];
float currents[NwMAX];

bool simulate = false;

int charge_to_color(float charge) {
   int v = charge * 255.0f;
   int r = min(v+255, 255), g = 255 - abs(v), b = min(255-v, 255);
   return (b & 0xFF) | ((g & 0xFF) << 8) | (r << 16);
}

int bool_to_color(unsigned char x) {
   x--;
   return (~x << 8) | (x << 16);
}

int main() {
   SDL_Window *window;
   SDL_Surface *surface;

   SDL_Init(SDL_INIT_VIDEO);

   window = SDL_CreateWindow("Test", SCREEN_SIZE, SCREEN_SIZE, 0);
   surface = SDL_GetWindowSurface(window);

   Nn = 31;
   Nw = 30;

   for (int i = 0; i < Nn; i++) {
      mass[i] = 1.0f;
      charges[i] = 0.0f;
   }

   for (int i = 0; i < 10; i++) {
      nodes[i] = (Vec2){HSS - 120, HSS + i * 12};
      nodes[30 - i] = (Vec2){HSS + 120, HSS + i * 12};
   }

   for (int i = 0; i < 10; i++) {
      nodes[i + 10] = (Vec2){HSS - 120 + i * 12, HSS + 120};
   }

   nodes[20] = (Vec2){HSS + 120, HSS + 120};

   for (int i = 0; i < Nw; i++) {
      inv_resistances[i] = 1.0f;
      wires[i] = (Vec2){i, i+1};
      int dx = nodes[i+1].x - nodes[i].x, dy = nodes[i+1].y - nodes[i].y;
      inv_length[i] = 10.0f / SDL_sqrtf(dx * dx + dy * dy);
   }

   const float infinity;
   *(int*)&infinity = 0xFF << 23;

   mass[0] = infinity;
   mass[Nn-1] = infinity;

   charges[0] = 1.0f;
   charges[Nn-1] = -1.0f;

   inv_resistances[19] = 0.1f;

   for (int i = 0; i < Nn; i++) {
      inv_mass[i] = 1.0f / mass[i];
   }

   SDL_Event e;
loop0:
   while (SDL_PollEvent(&e)) {
      if (e.type == SDL_EVENT_QUIT) goto exit0;
      if (e.type == SDL_EVENT_KEY_DOWN) {
         if (e.key.key == SDLK_SPACE)
            simulate ^= 1;
         }
   }

   fill_surface(surface, 0);

   // ---< /simulate >---

   if (simulate) {
      for (int i = 0; i < Nw; i++) {
         Vec2 wire = wires[i];
         currents[i] = (charges[wire.x] - charges[wire.y]) * inv_resistances[i] * inv_length[i];
      }
      
      for (int i = 0; i < Nn; i++)
         charges[i] -= mul2pow(charges[i], log2_dt-8) * inv_mass[i]; // Decay
      
      for (int i = 0; i < Nw; i++) {
         Vec2 wire = wires[i];
         float currentdt = mul2pow(currents[i], log2_dt);
         charges[wire.x] -= currentdt * inv_mass[wire.x];
         charges[wire.y] += currentdt * inv_mass[wire.y];
      }
   }

   // ---< simulate/ >---

   // ---< /draw gui >---

   rectangle(surface, 0, 0, 20, 20, bool_to_color(simulate), true);

   // ---< draw gui/ >---

   // ---< /draw sim >---

   for (int i = 0; i < Nw; i++) {
      Vec2 wire = wires[i], node1 = nodes[wire.x], node2 = nodes[wire.y];
      int shade = min(abs((int)mul2pow(currents[i], 9)), 0xFF);
      drawline(surface, node1.x, node1.y, node2.x, node2.y, (shade << 8) | 0x100010, 1 + (int)mul2pow(inv_resistances[i], 3));
   }

   for (int i = 0; i < Nn; i++) {
      Vec2 node = nodes[i];
      int color = charge_to_color(charges[i]);
      float invm = inv_mass[i];
      if (invm) drawcircle(surface, node.x, node.y, 7.0f-mul2pow(invm, 2), color);
      else rectangle(surface, node.x - 4, node.y - 4, node.x + 4, node.y + 4, color, 0);
   }

   // ---< draw sim/ >---

   SDL_UpdateWindowSurface(window);
   if (!simulate) SDL_Delay(50);

   goto loop0;
exit0:

   SDL_DestroyWindow(window);

   SDL_Quit();
   return 0;
}