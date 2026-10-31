# Mempass 密码管理器

基于 C++ / Qt 6  的密码管理器工具，包含可独立运行的 Windows 客户端和可选的同步服务端。

![截图](https://github.com/user-attachments/assets/66a134f4-115a-4202-b576-dc25a6f1d9cd)

## 特性

- 端到端加密，主密码只用于本地解锁保险库。
- 支持分类和账号管理、搜索、账号状态、随机密码生成功能。
- 使用 Argon2id 主密码派生，数据 XChaCha20-Poly1305 认证加密。
- 同步服务端只保存密文记录，不支持重置或找回主密码。
- 记录级 LWW 同步，支持删除墓碑、多设备增量同步和新设备恢复。
- 服务端基于 Qt HTTP Server 和 SQLite。

## 工程模块

```text
common/                 共享模型、同步逻辑
client/core/            加密、本地保险库、同步网络服务
client/ui/              Qt Widgets 客户端
server/                 Qt 同步服务端
tests/                  Qt Test 单元与集成测试
```

## Windows 客户端构建

### 前置条件

- Qt 6（MinGW 版，这里使用 Qt 6.10.x MinGW 64-bit）
- MinGW 与 Ninja（Qt 在线安装器自带的 Tools 中通常已包含）
- vcpkg，并设置环境变量 `VCPKG_ROOT` 指向 vcpkg 根目录
- MinGW、Ninja、Qt 的 `bin` 目录在 `PATH` 中，或配置时通过参数显式指定

本项目通过 vcpkg manifest 自动安装静态 libsodium（triplet：`x64-mingw-static`），正常构建无需额外复制 libsodium DLL。

### 构建程序

Windows下执行以下命令构建：

```powershell
cmake --preset windows-mingw-release
cmake --build --preset windows-mingw-release
```

如果 Qt 不在默认搜索路径，配置时额外指定其位置，例如：

```powershell
cmake --preset windows-mingw-release -DCMAKE_PREFIX_PATH=C:\Qt\6.10.1\mingw_64
```

如需固定的个人配置（本机 Qt/vcpkg 路径等），可在工程根目录创建 `CMakeUserPresets.json` 定义自己的 preset（例如命名为 `local`）。

使用 preset 构建：

```powershell
cmake --preset local
cmake --build --preset local
ctest --preset local
```

不使用 preset 构建：

```powershell
cmake -S . -B build/release -G Ninja `
    -DCMAKE_BUILD_TYPE=Release `
    -DCMAKE_PREFIX_PATH=C:\Qt\6.10.1\mingw_64 `
    -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake" `
    -DVCPKG_TARGET_TRIPLET=x64-mingw-static
cmake --build build/release
```

客户端输出文件通常在 `build/windows-mingw-release/client/ui/mempass.exe`（preset 方式）或 `build/release/client/ui/mempass.exe`。

发布前运行自动依赖收集程序：

```powershell
windeployqt --release build\windows-mingw-release\client\ui\mempass.exe
```

## Linux 同步服务端构建

在服务器安装 Qt 6 开发包，要求系统至少包含 Core、Network、HttpServer、Sql，以及 SQLite 开发文件。

配置、构建服务端：

```bash
cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release
cmake --build build/release --target mempass_server
```

## 初始化服务端

```bash
./build/release/server/mempass_server --init --data-dir /var/lib/mempass --host 0.0.0.0 --port 18765
```

初始化会生成：

- `/var/lib/mempass/server.json`
- `/var/lib/mempass/vault.db`
- 一次性的随机 Bearer 访问令牌

请立即保存终端中打印的令牌。服务端只保存令牌 SHA-256 摘要，忘记令牌后需要重新初始化或手工替换配置中的摘要。

启动服务端：

```bash
./build/release/server/mempass_server --config /var/lib/mempass/server.json
```

注意：开发环境可以使用 HTTP，公网或不可信网络部署必须启用 HTTPS，这需要在服务端配置中提供证书和私钥，或使用 Nginx/Caddy 等反向代理提供 HTTPS 入口，并将服务端绑定到本机地址。

## 服务端参数说明

### 命令行选项

| 选项 | 说明 | 默认值 |
|---|---|---|
| `--init` | 初始化模式：创建数据目录、数据库并生成一次性访问令牌，初始化完成后退出 | - |
| `--data-dir <dir>` | 数据目录，初始化时在该目录下生成 `server.json` 与 `vault.db`；`--init` 时必填 | - |
| `--host <host>` | 监听地址，必须是合法 IP（如 `127.0.0.1`、`0.0.0.0`） | `127.0.0.1` |
| `--port <port>` | 监听端口，范围 1–65535 | `8080` |
| `--config <file>` | 配置文件路径 | `./server.json` |
| `-h`, `--help` | 显示帮助信息 | - |

说明：

- 带值的选项同时支持两种写法：`--port 18765` 或 `--port=18765`。
- `--host` 与 `--port` 只在初始化时写入配置，日常启动以配置文件中的值为准。

### 配置文件 `server.json`

初始化生成的配置文件包含以下字段：

| 字段 | 说明 |
|---|---|
| `vaultId` | 保险库 UUID（32 位十六进制），标识本服务端对应的保险库，客户端恢复时使用 |
| `tokenHash` | 访问令牌的 SHA-256 十六进制摘要；服务端不保存明文令牌，启动时必填 |
| `host` | 监听地址，默认 `127.0.0.1`；对外提供服务时通常改为 `0.0.0.0` |
| `port` | 监听端口，默认 `8080` |
| `dbPath` | SQLite 数据库文件路径 |
| `certPath` | TLS 证书文件路径，与 `keyPath` 同时配置时启用 HTTPS，同时为空时使用 HTTP |
| `keyPath` | TLS 私钥文件路径，必须与 `certPath` 成对配置 |

校验规则：

- `vaultId` 必须是 32 位十六进制字符串。
- `tokenHash` 不能为空，否则拒绝启动。
- `certPath` 与 `keyPath` 必须同时配置或同时为空。
- 修改 `host`、`port`、`certPath`、`keyPath` 后重启服务端生效，无需重新初始化。

## 客户端使用

1. 第一次运行时在“新建本地保险库”页输入主密码。
2. 另一台设备可从“从同步服务恢复”页输入服务器地址、访问令牌和相同主密码。
3. 在“同步设置”中保存服务器地址和令牌。

Windows下，本地数据默认位于磁盘中以下目录：

```text
%APPDATA%\gacfox\mempass\vault.mempass
```

每次成功加载前的最近版本会保留为 `vault.mempass.bak`。

## 同步模型

- 分类和账号均以 UUID 作为跨设备 ID。
- 每条记录携带 `updatedAtMs` 和 `deviceId`。
- LWW 先比较 UTC 毫秒时间，再比较设备 ID，保证各端确定收敛。
- 删除保存为墓碑记录，不会在客户端之间复活。
- 分类删除会将账号移动到默认分类，分类和账号变化分别参与 LWW。
- 修改主密码只更新加密密钥信封，不重新加密所有账号记录。

## 重要限制

- 服务端不能重置主密码，遗忘主密码将无法恢复数据。
- 同步冲突按整条记录解决，不做字段级合并。
- 客户端时钟严重不准可能影响 LWW 结果。
