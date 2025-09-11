$ $
$ SPDX-License-Identifier: BSD-2-Clause
$ $
$ Copyright (c) 2018-2025 Gavin D. Howard and contributors.
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

$ 错误类型。
$set 1

1 "数学错误："
2 "解析错误："
3 "运行时错误："
4 "致命错误："
5 "警告："

$ 数学错误。
$set 2

1 "负数"
2 "非整数"
3 "溢出：数字不符合硬件号码"
4 "除以0"

$ 解析错误。
$set 3

1 "文件结束"
2 "无效字符'%c'"
3 "找不到字符串尾部"
4 "无法找到评论的结尾"
5 "无效令牌"
6 "无效表达"
7 “空表达”
8 "无效的打印或流语句"
9 "无效的功能定义"
10 "无效分配：左侧必须是scale、ibase、obase、last、var或数组元素"
11 "没有找到自动变量"
12 "函数参数或自动参数 \"%s%s\" 已经存在"
13 "找不到区块末端"
14 "不能从void函数中返回一个值：%s()"
15 "var不能作为参考：%s"
16 "POSIX不允许名字超过1个字符：%s"
17 "POSIX不允许'#'脚本注释"
18 "POSIX不允许使用以下关键字：%s"
19 "POSIX不允许用句号('.')作为最后结果的快捷方式"
20 "POSIX要求在返回表达式周围加括号"
21 "POSIX不允许以下操作符：%s"
22 "POSIX不允许在if语句或循环之外的比较运算符"
23 "POSIX要求每个条件的比较运算符为0或1个"
24 "POSIX要求for循环的所有3个部分必须是非空的"
25 "POSIX要求在分号和函数定义之间使用换行符"
26 "POSIX不允许使用指数符号"
27 "POSIX不允许数组引用作为函数参数"
28 "POSIX不允许无效函数"
29 "POSIX要求左边的括号和函数头在同一行上"
30 "POSIX不允许将字符串分配给变量或数组"

$ 运行时错误。
$set 4

1 "无效的ibase: 必须是[%lu, %lu]"
2 "无效的obase：必须是[%lu，%lu]"
3 "无效的scale：必须是[%lu，%lu]"
4 "无效的read()表达式"
5 "递归读取()调用"
6 "变量或数组元素是错误的类型"
7 "堆栈的元素太少"
8 "寄存器 \"%s\" 的堆栈的元素太少"
9 "参数数量错误；需要%zu，有%zu"
10 "未定义的函数：%s()"
11 “不能在表达式中使用空值”

$ 致命错误。
$set 5

1 "内存分配失败"
2 "I/O错误"
3 "无法打开文件：%s"
4 "文件不是文本：%s"
5 "路径是一个目录：%s"
6 "无效的命令行选项：\"%s\""
7 "选项需要一个参数：'%c'(\"%s\")"
8 "选项不需要参数：'%c'(\"%s\")"
9 "无效的命令行选项参数：\"%s\""
