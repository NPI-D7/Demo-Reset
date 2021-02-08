#include "gfx.hpp"
#include "common.hpp"
#include "app.hpp"

extern C2D_SpriteSheet sprites;
void GFX::DrawTop(){
    Gui::ScreenDraw(Top);
    Gui::DrawSprite(sprites, 1, 0, 0);
}
void GFX::DrawBottom(){
    Gui::ScreenDraw(Bottom);
    Gui::DrawSprite(sprites, 2, 0, 0);
    Gui::DrawSprite(sprites, 3, 0, 0, 0.03f, 0.03f);
}
