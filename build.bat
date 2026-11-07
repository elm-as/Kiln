@echo off
setlocal
cd /d "%~dp0"

rem build.bat            the game, debug, with a console for printf
rem build.bat release    optimized game + kpack + build\game.pak, no console
rem build.bat kpack      just the asset packer
rem build.bat tests      unit tests, runs them too
rem build.bat breakout   the old breakout sample
rem build.bat mini       the small platformer sample
rem build.bat mini release  same, optimized, no console, + build\mini.pak to share

if not exist build mkdir build

set WARN=-Wall -Wextra -Wno-missing-field-initializers
set DEBUG=-std=c++17 -g -O1 %WARN%
set RELEASE=-std=c++17 -O2 -s -DNDEBUG %WARN%
set LIBS=-lopengl32 -lgdi32 -lwinmm -static

set COOK=src\cook.cpp src\cook_pix.cpp src\cook_sprites.cpp src\cook_audio.cpp ^
 src\fs_win32.cpp src\deflate.cpp src\image.cpp src\json.cpp src\json_write.cpp ^
 src\script.cpp src\script_parse.cpp src\script_stmt.cpp src\script_vm.cpp src\script_std.cpp ^
 src\synth.cpp src\synth_song.cpp

set ENGINE=%COOK% src\actions.cpp src\anim.cpp src\assets.cpp src\audio.cpp src\camera.cpp ^
 src\components.cpp src\editor.cpp src\editor_gizmos.cpp src\editor_panels.cpp src\editor_paint.cpp ^
 src\engine.cpp src\font_win32.cpp src\gamepad_win32.cpp src\gl.cpp src\parallax.cpp src\particles.cpp ^
 src\physics.cpp src\physics_step.cpp src\platform_win32.cpp src\render.cpp src\render_draw.cpp ^
 src\scene.cpp src\script_api.cpp src\scripting.cpp src\tilemap.cpp src\touch.cpp src\ui.cpp src\ui_widgets.cpp

set GAME=game\cutscene.cpp game\dialogue.cpp game\draw.cpp game\hud.cpp game\main.cpp game\menus.cpp ^
 game\natives.cpp game\options.cpp game\player.cpp game\player_combat.cpp game\relics.cpp ^
 game\save.cpp game\select.cpp game\thermal.cpp game\title.cpp game\world.cpp game\world_step.cpp

if "%1"=="release" goto release
if "%1"=="kpack" goto kpack
if "%1"=="tests" goto tests
if "%1"=="breakout" goto breakout
if "%1"=="mini" goto mini

echo building lueur (debug)
g++ %DEBUG% -Isrc -Igame %ENGINE% %GAME% -o build\lueur.exe %LIBS% || exit /b 1
goto :eof

:release
echo building lueur (release)
g++ %RELEASE% -mwindows -Isrc -Igame %ENGINE% %GAME% -o build\lueur.exe %LIBS% || exit /b 1
call :kpack || exit /b 1
build\kpack.exe assets build\game.pak || exit /b 1
goto :eof

:kpack
echo building kpack
g++ %RELEASE% -Isrc tools\kpack.cpp %COOK% -o build\kpack.exe -static || exit /b 1
goto :eof

:tests
echo building tests
g++ %DEBUG% -Isrc tests\tests.cpp %COOK% src\editor_paint.cpp src\actions.cpp src\camera.cpp -o build\tests.exe -static || exit /b 1
build\tests.exe
goto :eof

:breakout
echo building breakout
g++ %DEBUG% -Isrc samples\breakout.cpp %ENGINE% -o build\breakout.exe %LIBS% || exit /b 1
goto :eof

:mini
if "%2"=="release" goto mini_release
echo building mini
g++ %DEBUG% -Isrc samples\mini.cpp %ENGINE% -o build\mini.exe %LIBS% || exit /b 1
goto :eof

:mini_release
echo building mini (release)
g++ %RELEASE% -mwindows -Isrc samples\mini.cpp %ENGINE% -o build\mini.exe %LIBS% || exit /b 1
call :kpack || exit /b 1
build\kpack.exe samples\mini\assets build\mini.pak || exit /b 1
goto :eof
