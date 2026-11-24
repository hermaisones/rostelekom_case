# RectangleLab

Редактор фигур на C++17 и Qt 6. В нём можно добавлять, перемещать, соединять и удалять фигуры. Таблица поддерживает фильтры по номеру, типу и количеству фигур этого типа.

## Файлы проекта

- Запуск приложения, подключение к SQLite и подготовка схемы базы. [main.cpp](main.cpp)
- Объявления класса окна, элементов сцены и делегатов таблицы. [rectanglewindow.h](rectanglewindow.h)
- Интерфейс, работа с фигурами и связями, фильтры и отрисовка иконок количества. [rectanglewindow.cpp](rectanglewindow.cpp)
- Настройки сборки и подключение модулей Qt Widgets и Qt SQL. [CMakeLists.txt](CMakeLists.txt)

## Сборка и запуск

Готовая сборка находится в `build\rectangle_lab.exe`. Для пересборки из PowerShell в корне проекта нужны GCC, Qt 6, CMake и Ninja из MSYS2 UCRT64.

```powershell
$env:Path = "C:\msys64\ucrt64\bin;$env:Path"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
.\build\rectangle_lab.exe
```

## База данных

SQLite-файл находится в `%LOCALAPPDATA%\RectangleLab\rectangles.sqlite`. Таблица `rectangles` хранит ID, типы, координаты и размеры фигур. Поле `type_count` содержит общее количество фигур данного типа. В таблице `figure_links` хранятся пары ID соединённых фигур.
