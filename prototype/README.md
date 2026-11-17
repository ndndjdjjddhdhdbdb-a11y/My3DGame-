# Bolt Survival — этап 1

Отдельный настольный C++17-прототип на raylib. Эта папка не заменяет и не изменяет Android-версию игры в `app/`.

## Что реализовано

- FPS-камера с управлением мышью;
- перемещение WASD, бег на Shift;
- прыжок и гравитация;
- тестовая карта из примитивов raylib;
- прицел и простая надпись управления.

## Установка raylib на Ubuntu / Codespaces

```bash
sudo apt update
sudo apt install -y build-essential cmake git
git clone https://github.com/raysan5/raylib.git /tmp/raylib
cmake -S /tmp/raylib -B /tmp/raylib/build
cmake --build /tmp/raylib/build
sudo cmake --install /tmp/raylib/build
```

## Сборка прототипа

Из корня репозитория:

```bash
cmake -S prototype -B prototype/build
cmake --build prototype/build -j
./prototype/build/BoltSurvival
```

На Windows потребуется установленный CMake, компилятор C++17 и raylib, доступная через CMake package config.
