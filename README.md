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
