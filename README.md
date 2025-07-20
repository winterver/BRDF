# TODO
bug: Framerate is capped at 60.

bug: TextRenderPass::drawText() can't properly handle '\n'.

change: TextRenderPass::drawText()'s origin is left-bottom, and Y goes up.
change it to left-top and make Y go down.

# Compile

## Linux
```
sudo pacman -Sy glfw glm freetype2
meson setup build
meson compile -C build
```

## Windows
```
mkdir subprojects
meson wrap install glfw
meson wrap install glm
meson wrap install freetype2
meson setup build
meson compile -C build
```
