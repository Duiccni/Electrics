// C:\"Program Files"\LLVM\bin\clang.exe test.c -o test.exe -O -Iinclude -Llib -lSDL3

#include "SDL3/SDL.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

float mul2pow(float x, int n) {
   if (x == 0) return 0;
   *(int*)&x = *(int*)&x + (n << 23);
   return x;
}

typedef struct {
   float x, y;
} Vec2;

typedef struct {
   int x, y;
} Vec2i;

float dot(Vec2 v1, Vec2 v2) { return v1.x * v2.x + v1.y * v2.y; }
float magnitude_square(Vec2 v1) { return v1.x * v1.x + v1.y * v1.y; }

inline Vec2 Vec2_mul2pow(Vec2 v, int n) { return (Vec2){mul2pow(v.x, n), mul2pow(v.y, n)}; }
inline Vec2 Vec2_add(Vec2 v1, Vec2 v2) { return (Vec2){v1.x + v2.x, v1.y + v2.y}; }
inline Vec2 Vec2_sub(Vec2 v1, Vec2 v2) { return (Vec2){v1.x - v2.x, v1.y - v2.y}; }
inline Vec2 Vec2_mul(Vec2 v, float b) { return (Vec2){v.x * b, v.y * b}; }

#define SCREEN_SIZE 640
#define HALF_SCREEN_SIZE (SCREEN_SIZE >> 1)
#define VIEWPORT_SIZE 2

inline Vec2i Vec2_toscreen(Vec2 v) { return (Vec2i){(int)(v.x * (HALF_SCREEN_SIZE / VIEWPORT_SIZE)) + HALF_SCREEN_SIZE, (int)(v.y * (HALF_SCREEN_SIZE / VIEWPORT_SIZE)) + HALF_SCREEN_SIZE}; }

int log2_dt = -12;

void set_pixel(SDL_Surface *surface, Vec2i pos, int color) {
   if ((unsigned)pos.x >= surface->w || (unsigned)pos.y >= surface->h) return;
   ((int*)surface->pixels)[surface->w * pos.y + pos.x] = color;
}

void fill_surface(SDL_Surface *surface, int color) {
   int i = surface->w * surface->h;
   while (i) ((int*)surface->pixels)[--i] = color;
}

#define get_sign(x) (((x) >> 31) | 1)

void drawline(SDL_Surface *surface, int x0, int y0, int x1, int y1, int color) {
   int dx = x1 - x0, dy = y1 - y0;
   if (dx == 0) return;
   int cof = (dy << 8) / dx;

   int xo = 0, step = get_sign(dx);
   while (1) {
      set_pixel(surface, (Vec2i){x0 + xo, y0 + ((xo * cof) >> 8)}, color);
      if (xo == dx) return;
      xo += step;
   }
}

void drawdot(SDL_Surface *surface, Vec2i pos, int color) {
   set_pixel(surface, (Vec2i){pos.x, pos.y}, color);
   set_pixel(surface, (Vec2i){pos.x + 1, pos.y + 1}, color);
   set_pixel(surface, (Vec2i){pos.x + 1, pos.y - 1}, color);
   set_pixel(surface, (Vec2i){pos.x - 1, pos.y + 1}, color);
   set_pixel(surface, (Vec2i){pos.x - 1, pos.y - 1}, color);
}

#define Ne 1
#define Na 1

Vec2 atoms[Na];

Vec2 poss[Ne];
Vec2 vels[Ne];

Vec2 Forcedt(Vec2 delta) {
   float dist3 = magnitude_square(delta);
   dist3 = dist3 * sqrtf(dist3);
   return Vec2_mul2pow(Vec2_mul(delta, 1/dist3), log2_dt);
}

int main() {
   SDL_Window *window;
   SDL_Surface *surface;

   SDL_Init(SDL_INIT_VIDEO);

   window = SDL_CreateWindow("Test", SCREEN_SIZE, SCREEN_SIZE, 0);
   surface = SDL_GetWindowSurface(window);

   for (int i = 0; i < Ne; i++) {
      poss[i] = (Vec2){0, 0};
      vels[i] = (Vec2){0, 0};
   }
   
   poss[0] = (Vec2){1, 0};
   vels[0] = (Vec2){0, 1};
   // poss[1] = (Vec2){0.5, 0.5};

   for (int i = 0; i < Na; i++) {
      atoms[i] = (Vec2){0, 0};
   }

   SDL_Event e;
loop0:
   while (SDL_PollEvent(&e)) {
      if (e.type == SDL_EVENT_QUIT) goto exit0;
   }

   for (int i = 0; i < Ne; i++) {
      Vec2 pos = poss[i];
      for (int j = 0; j < i; j++) {
         Vec2 force = Forcedt(Vec2_sub(pos, poss[j]));
         vels[i] = Vec2_add(vels[i], force);
         vels[j] = Vec2_sub(vels[j], force);
      }
   }

   for (int i = 0; i < Ne; i++) {
      Vec2 pos = poss[i];
      for (int j = 0; j < Na; j++) {
         Vec2 force = Forcedt(Vec2_sub(pos, atoms[j]));
         vels[i] = Vec2_sub(vels[i], force);
      }
   }

   fill_surface(surface, 0);

   for (int i = 0; i < Ne; i++) {
      poss[i] = Vec2_add(poss[i], Vec2_mul2pow(vels[i], log2_dt));
      Vec2i pos = Vec2_toscreen(poss[i]);
      drawline(surface, pos.x, pos.y, pos.x + mul2pow(vels[i].x, 4), pos.y + mul2pow(vels[i].y, 4), 0x8080FF);
      drawdot(surface, pos, 0xFFFF00);
   }

   for (int i = 0; i < Na; i++) {
      drawdot(surface, Vec2_toscreen(atoms[i]), 0x00FFFF);
   }

   SDL_UpdateWindowSurface(window);
   // SDL_Delay(5);

   goto loop0;
exit0:

   SDL_DestroyWindow(window);

   SDL_Quit();
   return 0;
}