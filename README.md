# Sysycompiler
参考https://github.com/luklapse/sysyCompiler 对文件进行整合
将riscv.h和riscv.cpp还有astDump.cpp的内容移至main.cpp。
至于为啥不把riscv的函数弄成直接在最前面就定义好具体内容的形式，因为变量koopa ir的函数可能嵌套，难以确定函数执行顺序。
这么搞主要是按照北大在线文档，更多人可能想到的是只在main.cpp、ast.h这两个文件进行代码编写。
