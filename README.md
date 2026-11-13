AmeOS
=====

AmeOS — экспериментальная настольная система на базе FreeBSD с концепцией
собственного окружения AmeDE: верхняя панель, док, быстрый терминал и темы
Amaterasu, Sakura и OLED.

**Стадия: базовый консольный прототип.** Полная сборка компактного профиля
завершена; userland, GENERIC и ISO/USB-носители созданы и проверены по
контрольным суммам. Проверены загрузка носителей, офлайн-установка BIOS/UFS
и запуск установленной системы; ограничения указаны в протоколе.
Отдельно от консольного образа доступен [оконный прототип AmeDE](ameos/desktop/README.md)
на программном [графическом API](ameos/graphics/README.md).
См. [протокол валидации базового образа](ameos/docs/validation-2026-11.md).

- [Сайт проекта](https://ameos.umi-kujou.ru/)
- [Репозиторий](https://github.com/ntych/AmeOS)
- [Собственные компоненты AmeOS](ameos/README.md)
- [Инструкция сборки](ameos/docs/build.md)
- [Архитектура](ameos/docs/architecture.md)
- [План разработки и критерии готовности](ameos/docs/roadmap.md)

Текущая база — FreeBSD 16.0-CURRENT. Исходная ревизия зафиксирована в
[ameos/base-revision](ameos/base-revision); эта запись не обновляет исходники
автоматически. Первый профиль `base-amd64` использует ядро GENERIC и штатные
средства создания ISO/USB-образов без внешних пакетов. Полная сборка требует
отдельной FreeBSD/amd64 VM; Windows/Git Bash подходят для просмотра плана
и проверки управляющих скриптов.

Предварительный просмотр из корня клона в POSIX shell, без создания файлов:

```sh
sh ameos/scripts/build-release.sh --plan --workdir /var/tmp/ameos-base-001
```

Для этой команды родительский каталог должен существовать, а сам рабочий
каталог — ещё не существовать. Изменения AmeOS размещаются отдельно от
унаследованного дерева FreeBSD. Лицензии и уведомления исходных авторов
сохраняются; см. [COPYRIGHT](COPYRIGHT).

FreeBSD Source (upstream reference):
---------------
This is the top level of the FreeBSD source directory.

FreeBSD is an operating system used to power modern servers, desktops, and embedded platforms.
A large community has continually developed it for more than thirty years.
Its advanced networking, security, and storage features have made FreeBSD the platform of choice for many of the busiest web sites and most pervasive embedded networking and storage devices.

For copyright information, please see [the file COPYRIGHT](COPYRIGHT) in this directory.
Additional copyright information also exists for some sources in this tree - please see the specific source directories for more information.

The Makefile in this directory supports a number of targets for building components (or all) of the FreeBSD source tree.
See build(7), config(8), [FreeBSD handbook on building userland](https://docs.freebsd.org/en/books/handbook/cutting-edge/#makeworld), and [Handbook for kernels](https://docs.freebsd.org/en/books/handbook/kernelconfig/) for more information, including setting make(1) variables.

For information on the CPU architectures and platforms supported by FreeBSD, see the [FreeBSD
website's Platforms page](https://www.freebsd.org/platforms/).

For official FreeBSD bootable images, see the [release page](https://download.freebsd.org/ftp/releases/ISO-IMAGES/).

Source Roadmap:
---------------
| Directory | Description |
| --------- | ----------- |
| bin | System/user commands. |
| cddl | Source code for third-party software under the Common Development and Distribution License. |
| contrib | Source code for third-party software. |
| crypto | Source code for cryptographic libraries and commands (see [crypto/README](crypto/README)). |
| etc | Template files for /etc. |
| include | System include files. |
| kerberos5 | Build system for Kerberos 5 (Heimdal). |
| krb5 | Build system for Kerberos 5 (MIT). |
| lib | System libraries. |
| libexec | System commands intended to be executed by other commands or daemons. |
| packages | Base system packages. |
| release | Makefiles and scripts used for building releases and VM images. |
| rescue | Build system for statically linked /rescue commands. |
| sbin | System commands. |
| secure | Build system for cryptographic libraries and commands (excluding Kerberos). |
| share | Shared resources. |
| stand | Boot loader sources. |
| sys | Kernel sources (see [sys/README.md](sys/README.md)). |
| targets | Support for experimental `DIRDEPS_BUILD` |
| tests | Tests which can be run by Kyua.  See [tests/README](tests/README) for additional information. |
| tools | Ancillary utilities and tests (not included in the build). |
| usr.bin | User commands. |
| usr.sbin | System administration commands. |

For information on synchronizing your source tree with one or more of the FreeBSD Project's development branches, please see [FreeBSD Handbook](https://docs.freebsd.org/en/books/handbook/cutting-edge/#current-stable).
