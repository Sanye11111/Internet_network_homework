# 实验五说明

## 一、文件说明

- `license_server.cpp`：许可证发放服务器，端口 `9000`
- `status_server.cpp`：许可证校验和心跳服务器，端口 `9001`
- `client.cpp`：控制台客户端

## 二、功能说明

本程序分为两个服务器：

1. 许可证发放服务器
   - 管理员输入用户名、口令、许可证类型
   - 服务器返回一个 10 位序列号
2. 状态报送服务器
   - 客户端输入序列号后连接服务器
   - 服务器根据该序列号对应的上限判断是否允许使用
   - 客户端会定期发送心跳，退出时发送注销指令

说明：

- `type` 输入 `10` 表示 10 人许可证
- `type` 输入 `50` 表示 50 人许可证
- 序列号第一次申请后可直接在客户端输入复用

## 三、编译方法

### 方法 1：MinGW / g++

在 `D:\vscode\Internet_network\Internet_network_homework\lab5` 目录下执行：

```bash
g++ -std=c++17 -O2 -o license_server.exe license_server.cpp -lws2_32
g++ -std=c++17 -O2 -o status_server.exe status_server.cpp -lws2_32
g++ -std=c++17 -O2 -o client.exe client.cpp -lws2_32
```

### 方法 2：Visual Studio

新建 3 个控制台项目，分别加入对应的 `.cpp` 文件，并链接 `Ws2_32.lib`。

## 四、运行步骤

必须按下面顺序启动：

1. 先启动 `license_server.exe`
2. 再启动 `status_server.exe`
3. 最后启动 `client.exe`

## 五、客户端输入说明

启动客户端后，会出现：

```text
1. request new license
2. use existing license
choice:
```

### 情况 1：申请新许可证

输入：

```text
1
```

然后按顺序输入：

```text
username: 用户名
password: 口令
type (10/50): 10 或 50
```

客户端会显示服务器返回的序列号，例如：

```text
serial: 4117359001
```

### 情况 2：使用已有许可证

输入：

```text
2
```

然后输入已有序列号：

```text
serial: 4117359001
```

## 六、正常运行效果

客户端连接成功后会显示：

```text
authorization success: ALLOW|30
press ENTER to exit
```

此时客户端已经获得授权。

- 按 `Enter` 可正常退出
- 退出时会向服务器发送 `LOGOUT`
- 服务器会删除该在线记录

## 七、协议格式

- 申请许可证：`ISSUE|username|password|type`
- 认证请求：`AUTH|serial|client_id|limit`
- 心跳报送：`PING|serial|client_id`
- 正常退出：`LOGOUT|serial|client_id`

## 八、测试建议

1. 先申请一个 `10` 人许可证。
2. 再用同一个序列号启动客户端。
3. 观察状态服务器输出的授权、心跳、退出日志。
4. 多开几个客户端测试并发。

## 九、常见问题

- 连接失败：确认两个服务器都已经启动。
- 认证失败：确认输入的序列号正确。
- 端口占用：检查 `9000` 和 `9001` 是否被其他程序占用。
