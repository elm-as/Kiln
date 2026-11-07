#pragma once
#include "game.h"

enum TitleItem {
    ITEM_CONTINUE,
    ITEM_PLAY,
    ITEM_WORLDS,
    ITEM_OPTIONS,
    ITEM_EDITOR,
    ITEM_QUIT
};

void title_build_menu();
int title_update(const Input& in, float dt);
void draw_title();
