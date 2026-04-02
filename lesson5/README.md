# lesson5

第 35 到 37 题的代码已经分别写到下面这些文件里：

- `q35_mac_address_match.cpp`
- `q36_ethernet_frame_header.c`
- `q37_csma_cd_simulation.cpp`

在 MinGW / g++ 下可以这样编译：

```bash
g++ q35_mac_address_match.cpp -o q35
gcc q36_ethernet_frame_header.c -o q36
g++ q37_csma_cd_simulation.cpp -o q37 -pthread
```

如果你是在 Windows 的 MSVC 环境里编译，第 36 题不需要运行，能通过语法检查即可。
