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

1 "Erro de cálculo:"
2 "Erro de análise de sintaxe:"
3 "Erro de execução:"
4 "Erro fatal:"
5 "Aviso:"

$ Math errors.
$set 2

1 "número negativo"
2 "número não inteiro"
3 "Estouro: número não cabe no registro"
4 "dividir por 0"

$ Parse errors.
$set 3

1 "fim do arquivo"
2 "caractere inválido '%c'"
3 "Não foi possível encontrar o final da string"
4 "Não foi possível encontrar o final do comentário"
5 "token inválido"
6 "expressão inválida"
7 "expressão vazia"
8 "instrução de gravação ou de fluxo inválida"
9 "definição de função inválida"
10 "atribuição inválida: a parte esquerda deve ser 'scale', 'ibase', 'obase', 'last', uma variável ou um elemento da matriz"
11 "nenhuma variável automática encontrada"
12 "parâmetro de função ou variável automática \"%s%s\" já existe"
13 "fim do bloco não encontrado"
14 "uma função 'void' não pode retornar um valor: %s()"
15 "Uma variável não pode ser uma referência: %s"
16 "POSIX não permite nomes com mais de 1 caractere: %s"
17 "POSIX não permite comentários de script '#'"
18 "POSIX não permite a seguinte palavra-chave: %s"
19 "POSIX não permite um ponto ('.') como um atalho para o último resultado"
20 "POSIX requer parênteses em torno de expressões de retorno"
21 "POSIX não permite o seguinte operador: %s"
22 "POSIX não permite operadores de comparação fora das expressões 'if' ou loops"
23 "POSIX requer operadores 0 ou 1 de comparação por condição"
24 "POSIX não permite uma expressão vazia em um loop 'for'"
25 "POSIX requer uma nova linha entre um ponto-e-vírgula e uma definição de função"
26 "POSIX não permite notação exponencial"
27 "POSIX não permite referências de matriz como parâmetros de função"
28 "POSIX não permite funções nulas"
29 "POSIX requer que o cabeçalho da função '{' estejam na mesma linha"
30 "POSIX não permite a atribuição de cadeias de caracteres a variáveis ou matrizes"

$ Runtime errors.
$set 4

1 "ibase inválido: deve ser [%lu, %lu]"
2 "obase inválido: deve ser [%lu, %lu]"
3 "scale inválida: deve ser [%lu, %lu]"
4 "expressão read() inválida"
5 "chamada read() recursiva"
6 "tipo errado de variável ou elemento de matriz"
7 "pilha tem poucos elementos"
8 "pilha para registo \"%s\" tem poucos elementos"
9 "número incorreto de parâmetros - esperado: %zu, obtido: %zu"
10 "função indefinida: %s()"
11 "um valor 'void' não pode ser usado em uma expressão"

$ Fatal errors.
$set 5

1 "falha na alocação de memória"
2 "erro de entrada-saída"
3 "impossível abrir o arquivo: %s"
4 "arquivo não é texto: %s"
5 "caminho é um diretório: %s"
6 "opção de linha de comando inválida: \"%s\""
7 "opção requer um argumento: '%c' (\"%s\")"
8 "a opção não aceita argumentos: '%c' (\"%s\")"
9 "argumento de opção de linha de comando inválido: \"%s\""
