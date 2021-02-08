#include "gfx.hpp"
#include "common.hpp"
#include "app.hpp"

extern C2D_SpriteSheet sprites;
void GFX::DrawTop(){
    Gui::ScreenDraw(Top);
    Gui::DrawSprite(sprites, sprites_bgrtop_idx, 0, 0);
}
void GFX::DrawBottom(){
    Gui::ScreenDraw(Bottom);
    Gui::DrawSprite(sprites, sprites_bgrbo_idx, 0, 0);
    Gui::DrawSprite(sprites, sprites_universal-core-logo_idx, 0, 0, 0.03f, 0.03f);
}
