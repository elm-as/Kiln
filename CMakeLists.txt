cmake_minimum_required(VERSION 3.16)
project(kiln CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# the asset cooker is shared by the engine (dev hot reload) and kpack
add_library(kiln_cook STATIC
    src/cook.cpp
    src/cook_pix.cpp
    src/cook_sprites.cpp
    src/cook_audio.cpp
    src/fs_win32.cpp
    src/deflate.cpp
    src/image.cpp
    src/json.cpp
    src/json_write.cpp
    src/script.cpp
    src/script_parse.cpp
    src/script_stmt.cpp
    src/script_vm.cpp
    src/script_std.cpp
    src/synth.cpp
    src/synth_song.cpp
)
target_include_directories(kiln_cook PUBLIC src)

add_library(kiln STATIC
    src/actions.cpp
    src/anim.cpp
    src/assets.cpp
    src/audio.cpp
    src/camera.cpp
    src/components.cpp
    src/editor.cpp
    src/editor_gizmos.cpp
    src/editor_panels.cpp
    src/editor_paint.cpp
    src/engine.cpp
    src/font_win32.cpp
    src/gamepad_win32.cpp
    src/gl.cpp
    src/parallax.cpp
    src/particles.cpp
    src/physics.cpp
    src/physics_step.cpp
    src/platform_win32.cpp
    src/render.cpp
    src/render_draw.cpp
    src/scene.cpp
    src/script_api.cpp
    src/scripting.cpp
    src/tilemap.cpp
    src/touch.cpp
    src/ui.cpp
    src/ui_widgets.cpp
)
target_link_libraries(kiln PUBLIC kiln_cook opengl32 gdi32 winmm)

add_executable(lueur
    game/cutscene.cpp
    game/dialogue.cpp
    game/draw.cpp
    game/hud.cpp
    game/main.cpp
    game/menus.cpp
    game/natives.cpp
    game/options.cpp
    game/player.cpp
    game/player_combat.cpp
    game/relics.cpp
    game/save.cpp
    game/select.cpp
    game/thermal.cpp
    game/title.cpp
    game/world.cpp
    game/world_step.cpp
)
target_include_directories(lueur PRIVATE game)
target_link_libraries(lueur PRIVATE kiln)

add_executable(kpack tools/kpack.cpp)
target_link_libraries(kpack PRIVATE kiln_cook)

add_executable(tests tests/tests.cpp src/editor_paint.cpp src/actions.cpp src/camera.cpp)
target_link_libraries(tests PRIVATE kiln_cook)

add_executable(breakout samples/breakout.cpp)
target_link_libraries(breakout PRIVATE kiln)

add_executable(mini samples/mini.cpp)
target_link_libraries(mini PRIVATE kiln)
