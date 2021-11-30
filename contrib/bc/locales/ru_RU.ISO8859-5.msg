$ $
$ SPDX-License-Identifier: BSD-2-Clause
$ $
$ Copyright (c) 2018-2021 Gavin D. Howard and contributors.
$ $
$ Redistribution and use in source and binary forms, with or without
$ modification, are permitted provided that the following conditions are met:
$ $
$ * Redistributions of source code must retain the above copyright notice, this
$   list of conditions and the following disclaimer.
$ $
$ * Redistributions in binary form must reproduce the above copyright notice,
$   this list of conditions and the following disclaimer in the documentation
$   and/or other materials provided with the distribution.
$ $
$ THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
$ AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
$ IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
$ ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
$ LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
$ CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
$ SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
$ INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
$ CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
$ ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
$ POSSIBILITY OF SUCH DAMAGE.
$ $

$quote "

$ Разные сообщения.
$set 1

1 "Функция:"

$ Типы ошибок.
$set 2

1 "Математическая ошибка:"
2 "Ошибка при разборе:"
3 "Ошибка выполнения:"
4 "Фатальная ошибка:"
5 "Предупреждение:"

$ Математические ошибки.
$set 3

1 "отрицательное число"
2 "неинтегрированное число"
3 "переполнение: номер не помещается в аппаратный номер"
4 "делить на 0"

$ Ошибки при разборе.
$set 4

1 "конец файла"
2 "недопустимый символ '%c'"
3 "конец строки не найден"
4 "конец комментария не найден"
5 "недействительный жетон"
6 "неправильное выражение"
7 "пустое выражение"
8 "заявление о недействительности печати или потока"
9 "определение недействительной функции"
10 "неверное присвоение: левая сторона должна быть scale, ibase, obase, last, варом или элементом массива"
11 "автоматическая переменная не найдена"
12 "параметр функции или auto \"%s%s\" уже существует"
13 "конец блока не найден"
14 "не может вернуть значение из функции void: %s()"
15 "var не может быть ссылкой: %s"
16 "POSIX не допускает имен длиннее 1 символа: %s"
17 "POSIX не допускает комментариев к сценарию '#'"
18 "POSIX не допускает следующее ключевое слово: %s"
19 "POSIX не допускает точку ('.') в качестве ярлыка для последнего результата"
20 "POSIX требует скобок вокруг выражений возврата"
21 "POSIX не разрешает использовать следующий оператор: %s"
22 "POSIX не разрешает операторам сравнения выходить за пределы, если утверждения или циклы"
23 "POSIX требует 0 или 1 оператора сравнения на условие"
24 "POSIX требует, чтобы все 3 части петли были непустыми"
25 "POSIX требует наличия новой строки между точкой с запятой и определением функции"
26 "POSIX не допускает экспоненциальной нотации"
27 "POSIX не допускает ссылки на массив в качестве параметров функции"
28 "POSIX не разрешает функции пустоты"
29 "POSIX требует, чтобы левая скобка была на той же линии, что и заголовок функции"
30 "POSIX не позволяет присваивать строки переменным или массивам"

$ Ошибки выполнения.
$set 5

1 "Недействительный ibase: должен быть [%lu, %lu]"
2 "Недействительный obase: должен быть [%lu, %lu]"
3 "недействительная scale: должна быть [%lu, %lu]"
4 "недействительное выражение read()"
5 "рекурсивный вызов read()"
6 "переменная или элемент массива является неправильным типом"
7 "стопка имеет слишком мало элементов"
8 "стопка имеет для регистра \"%s\" слишком мало элементов"
9 "неправильное количество параметров; нужно %zu, нужно %zu"
10 "неопределенная функция: %s()"
11 "не может использовать пустое значение в выражении"

$ Фатальные ошибки.
$set 6

1 "Не удалось выделить память"
2 "Ошибка ввода/вывода"
3 "не смог открыть файл: %s"
4 "файл не является текстовым: %s"
5 "путь - это каталог: %s"
6 "неверная опция командной строки: \"%s\""
7 "опция требует аргумента: '%c' (\"%s\")"
8 "опция не принимает аргументов: '%c' (\"%s\")"
9 "неверный аргумент опции командной строки: \"%s\""
