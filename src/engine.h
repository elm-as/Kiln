#pragma once
#include "platform.h"
#include "render.h"

struct GameDesc {
    const char* title;
    int width, height;          // canvas size
    int scale;                  // starting window size is width*scale by height*scale
    int tick_rate;              // fixed updates per second, 60 if left at 0
    Color clear;

    void (*init)();
    void (*frame)(const Input& in, float dt);     // once per rendered frame, before the ticks. menus, editor
    void (*update)(const Input& in, float dt);    // fixed rate
    void (*draw)(float alpha);  // alpha = how far we are between the last tick and the next one
    void (*shutdown)();
};

int engine_run(const GameDesc& g);
void engine_quit();

// stop the simulation for a moment, keeps drawing. Sells a hit.
void engine_hitstop(float seconds);
double engine_time();
uint64_t engine_ticks();

float randf(float lo, float hi);
int randi(int lo, int hi);      // inclusive
