# AmeDE Session Scripts

Скрипты для запуска и проверки AmeDE в Wayland-окружении.

## amede-session

Скрипт запуска полной пользовательской сессии AmeDE на Wayland-композиторе.

**Требования:**
- Один из композиторов: wayfire, labwc или weston
- `amede` в PATH
- `foot` для терминала (или переменная `AMEDE_TERMINAL`)

**Использование:**

Из TTY:
```sh
sh /path/to/amede-session
```

Или через display manager — добавьте в `/usr/local/share/wayland-sessions/amede.desktop`:
```ini
[Desktop Entry]
Name=AmeDE
Comment=Amethyst Desktop Environment
Exec=/usr/local/bin/amede-session
Type=Application
DesktopNames=amede
```

**Что делает:**
1. Настраивает `XDG_RUNTIME_DIR` при отсутствии
2. Определяет доступный композитор (wayfire → labwc → weston)
3. Генерирует autostart-конфигурацию для запуска amede
4. Запускает композитор с AmeDE внутри

## validate-amede-vps.sh

Скрипт полной проверки сборки AmeDE на FreeBSD VPS.

**Переменные окружения:**
- `AMEOS_WORK_DIR` — рабочая директория (по умолчанию `/home/ameosbuild/ameos-work`)
- `AMEOS_OUTPUT_DIR` — выходная директория для артефактов

**Использование:**

Из каталога `ameos/` репозитория:
```sh
sh scripts/validate-amede-vps.sh
```

**Проверяет:**
1. Форматирование кода (`cargo fmt --check`)
2. Тесты `ame-gfx`
3. Сборку `amede`
4. Экспорт PNG-preview
5. Запуск под weston headless (если доступен)

**Результаты:**
- PNG-preview: `$OUTPUT_DIR/sakura.png`
- Screenshot Wayland: `$OUTPUT_DIR/amede-wayland.png`
- Логи: `$OUTPUT_DIR/*.log`
