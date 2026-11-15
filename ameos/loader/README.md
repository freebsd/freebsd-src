# ame-loader

Терминальный загрузчик AmeOS в стиле archinstall с визуальным оформлением и поддержкой ZFS Boot Environments.

## Управление

- `↑` / `↓` — перемещение по пунктам;
- `Enter` — выбор пункта;
- `Esc` — возврат назад или выход из загрузчика.

## Архитектура

Загрузчик построен на модульной архитектуре с вложенными экранами:

- **screen.rs** — трейт `Screen` и перечисление `Action` для навигации;
- **menu.rs** — компонент меню с поддержкой навигации клавишами и цветового оформления;
- **backend.rs** — абстракция `BootBackend` для операций загрузки;
- **theme.rs** — цветовая схема и ASCII-логотип AmeOS;
- **zfs.rs** — работа с ZFS Boot Environments через `bectl`;
- **main.rs** — навигационный стек и интеграция с backend.

## Визуальное оформление

Загрузчик использует цветовую схему AmeOS:

- **Cyan** — рамки и границы;
- **White** — заголовки и обычный текст;
- **Yellow** — выделенный пункт меню;
- **Green** — статусные сообщения;
- **Dark Grey** — подсказки навигации.

Главное меню отображает ASCII-логотип AmeOS.

## Меню

### Главное меню

1. Boot AmeOS — загрузка с профилем по умолчанию
2. Select kernel — выбор ядра AmeOS vs vanilla FreeBSD
3. Boot profiles — выбор профиля загрузки
4. Boot environments (ZFS) — выбор ZFS Boot Environment
5. Boot recovery environment — среда восстановления
6. Hardware diagnostics — диагностика оборудования
7. Snapshot info — информация о сборке
8. Power off — завершение работы

### Boot profiles

1. Default — стандартная загрузка
2. Single user mode — однопользовательский режим (`-s`)
3. Safe mode — безопасный режим с подробным выводом (`-v`)
4. Back to main menu — возврат в главное меню


### Select Kernel

Позволяет выбрать ядро для загрузки между AmeOS и чистым FreeBSD для диагностики проблем.

Доступные варианты:

1. **AmeOS** (`/boot/kernel/kernel`) — ядро AmeOS с кастомизациями и патчами
2. **FreeBSD (vanilla)** (`/boot/kernel.freebsd/kernel`) — чистое ядро FreeBSD без модификаций AmeOS
3. **FreeBSD (old)** (`/boot/kernel.old/kernel`) — предыдущая резервная копия ядра
4. **GENERIC (safe)** (`/boot/kernel.GENERIC/kernel`) — generic-ядро FreeBSD для восстановления

**Статусы ядер**:
- **(current)** — текущее загруженное ядро (из `sysctl kern.bootfile`)
- **(unavailable)** — ядро отсутствует на диске

**Назначение**:
- При возникновении ошибок в AmeOS можно загрузиться с vanilla FreeBSD
- Vanilla kernel не содержит надстроек AmeOS, что позволяет диагностировать источник проблемы
- Из-под чистого FreeBSD доступны системные логи для анализа

**Проверка доступности**:
- На FreeBSD: проверяет существование файла ядра через `Path::exists()`
- На других ОС: все ядра помечены как доступные для тестирования навигации

**Выбор ядра**:
- Выбранное ядро устанавливается в `FreeBSDBackend` через `set_kernel()`
- При следующей загрузке (Boot AmeOS, Boot profiles) будет использовано выбранное ядро
- Выбор сохраняется до перезапуска загрузчика

**Получение версии ядра**:
```rust
pub fn get_version(&self) -> io::Result<String>
```
Использует `/usr/bin/what <kernel>` для извлечения строки версии из бинаря ядра.

### Boot Environments (ZFS)

Динамическое меню, сформированное из выходных данных `bectl list`:

- Показывает все доступные Boot Environments;
- Активный BE отмечен как `(active)`;
- Выбор BE активирует его для следующей загрузки через `bectl activate`;
- Back to main menu — возврат в главное меню.

### Recovery Environment

1. Mount filesystems — монтирование root в режиме rw и всех ФС из fstab
2. Network configuration — автоматическая настройка через DHCP
3. Shell — запуск интерактивной recovery shell
4. Back to main menu — возврат в главное меню

### Hardware Diagnostics

1. Memory test — информация о памяти через `sysctl hw.physmem/usermem/realmem`
2. Disk test — список дисков через `geom disk list`
3. CPU information — данные CPU через `sysctl hw.model/ncpu/machine`
4. Back to main menu — возврат в главное меню

## Backend

Загрузчик автоматически выбирает backend в зависимости от платформы:

- **FreeBSD** — `FreeBSDBackend` с реальными системными вызовами;
- **Другие ОС** — `StubBackend` с stub-сообщениями для разработки.

### FreeBSDBackend

Использует следующие системные команды:

- `/sbin/reboot -k <kernel> <flags>` — загрузка ядра;
- `/sbin/mount -u -w /` и `/sbin/mount -a` — монтирование ФС;
- `/sbin/dhclient -b` — настройка сети;
- `/bin/sh` — recovery shell;
- `/sbin/sysctl` — системная информация;
- `/sbin/geom disk list` — информация о дисках;
- `/usr/sbin/bectl list -H` — список Boot Environments;
- `/usr/sbin/bectl activate <name>` — активация BE.

## ZFS Boot Environments

Модуль `zfs.rs` предоставляет интеграцию с ZFS Boot Environments:

```rust
pub struct BootEnvironment {
    pub name: String,
    pub active: bool,
    pub mountpoint: String,
    pub creation: String,
}

pub fn list_boot_environments() -> io::Result<Vec<BootEnvironment>>;
pub fn activate_boot_environment(name: &str) -> io::Result<()>;
```

На FreeBSD использует `bectl`, на других платформах возвращает stub-данные для разработки.

## Проверка

Из каталога `ameos/`:

```sh
cargo run -p ame-loader
```

Для выхода из любого меню нажмите `Esc`. Выход из главного меню завершает работу загрузчика.

## Интеграция в систему

Для встраивания загрузчика в систему сборки AmeOS:

1. Скомпилировать с `cargo build --release -p ame-loader`;
2. Установить бинарь в `/boot/loader/` или `/usr/local/sbin/`;
3. Настроить автоматический запуск загрузчика перед загрузкой ядра.

Реальная интеграция с FreeBSD loader и передача управления ядру будет реализована отдельным этапом.

### Snapshot Info

Отображает информацию о текущей сборке системы:

**Build number** — номер сборки ядра (через `uname -K` на FreeBSD):
- На FreeBSD: реальное значение из системы
- На других ОС: stub `16.0-CURRENT-20261113`

**Release date** — дата сборки ядра (через `date -r /boot/kernel/kernel`):
- На FreeBSD: время модификации файла ядра
- На других ОС: stub `2026-11-13 15:42:08`

**Moscow time** — текущее время в Москве (через `TZ=Europe/Moscow date`):
- На FreeBSD: реальное время с учётом часового пояса MSK (UTC+3)
- На других ОС: вычисленное время на основе system clock + UTC+3

Пример вывода:

```
AmeOS Snapshot Information
===========================

Build number:    16.0-CURRENT-20261113
Release date:    2026-11-13 15:42:08
Moscow time:     2026-11-13 21:34:56 MSK
```

Команда завершает работу загрузчика после нажатия Enter.
