$ $
$ SPDX-License-Identifier: BSD-2-Clause
$ $
$ Copyright (c) 2018-2023 Gavin D. Howard and contributors.
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

$ Error types.
$set 1

1 "Error de matemática:"
2 "Error de syntaxis:"
3 "Error de ejecución:"
4 "Error fatal:"
5 "Advertencia:"

$ Math errors.
$set 2

1 "número negativo"
2 "número no es entero"
3 "desbordamiento de enteros: no se puede encajar el el hardware"
4 "división por cero"

$ Parse errors.
$set 3

1 "fin de archivo"
2 "no válido '%c'"
3 "no puede encontrar el fine de la cadena"
4 "no puede encontrar el fine del comentario"
5 "el token no es válido"
6 "la expresión no es válida"
7 "la expresión es vacía"
8 "la expresión de print o de stream no es válida"
9 "la definición de función no es válida"
10 "la asignación no es valida: en la izquierda debe ser scale, ibase, obase, last, var, o un elemento de matriz"
11 "no se encontró ninguna variable automática"
12 "ya hay un parámetro de función o variable automatica que se llama \"%s%s\""
13 "no se puede encontrar el final de del bloque de código"
14 "no puede haber un valor de retorno de una función \"void\": %s()"
15 "var no puede ser una referencia: %s"
16 "POSIX no permite nombres de más de 1 carácter: %s"
17 "POSIX no permite '#' script comentarios"
18 "POSIX no permite este palabra clave %s"
19 "POSIX no permite un punto ('.') como un atajo del resultado previoso"
20 "POSIX requieres paréntesis en el expresión del \"return\""
21 "POSIX no permite este operador: %s"
22 "POSIX no permite operadores de comparación aparte de \"if\" expresión o bucles"
23 "POSIX requiere 0 o 1 operadores de comparisón para cada condición"
24 "POSIX requiere todos 3 partes de una bucla que no esta vacío"
25 "POSIX requiere una nueva línea entre un punto y coma y una definición de función"
26 "POSIX no permite una notación exponencial"
27 "POSIX no permite una referencia a una matriz como un parámetro de función"
28 "POSIX no permite funciones void"
29 "POSIX requiere el llave de la izquierda que sea en la misma línea que los parámetros de la función"
30 "POSIX no permite asignar cadenas a variables o matrices"

$ Runtime errors.
$set 4

1 "\"ibase\" no es válido: debe ser [%lu, %lu]"
2 "\"obase\" no es válido: debe ser [%lu, %lu]"
3 "\"scale\" no es válido: debe ser [%lu, %lu]"
4 "read() expresión no es válido"
5 "recursion en la invocación de read()"
6 "variable o elemento del matriz de tipo equivocado"
7 "la pila no ha demaciado elementos"
8 "la pila del registro \"%s\" no ha demaciado elementos"
9 "la función no tiene un número de argumentos correcto; necessita %zu, tiene %zu"
10 "la función no esta definida: %s()"
11 "no puede utilizar un valor vacío en una expresión"

$ Fatal errors.
$set 5

1 "error en la asignación de memoria"
2 "error de I/O"
3 "no puede abrir el archivo: %s"
4 "el archivo no es texto: %s"
5 "el ruta es un directorio: %s"
6 "una opción de línea de comandos no es válida: \"%s\""
7 "una opción requiere un argumento: '%c' (\"%s\")"
8 "una opción no tiene argumento: '%c' (\"%s\")"
9 "uno argumento de opción de línea de comandos no es válido: \"%s\""
