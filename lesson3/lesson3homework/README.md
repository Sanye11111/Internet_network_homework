# Message Sequence Demo

一个简单的 C++ 课程作业工程，包含两部分内容：

1. 两路消息序列的复用与解复用
2. 载波、调制信号与调频/调幅/调相信号生成

## 目录结构

```text
.
|-- CMakeLists.txt
|-- include
|   |-- modulation.h
|   `-- multiplex.h
`-- src
    |-- main.cpp
    |-- modulation.cpp
    `-- multiplex.cpp
```

## 构建方式

### 用 CMake

```bash
cmake -S . -B build
cmake --build build
```

生成的可执行文件名为 `message_sequence`。

## 说明

- 题目原文里出现了 `unsigned double`，但标准 C++ 中没有这个类型，所以工程里统一使用 `double`。
- 数字消息序列中，非 0 都按 1 处理。
- `main.cpp` 里给了一个最基本的演示，方便直接运行查看结果。
