# Compile

## Linux
```
sudo pacman -S glfw glm
meson setup build
meson compile -C build
```

## Windows
```
meson wrap install glfw
meson wrap install glm
meson setup build
meson compile -C build
```
