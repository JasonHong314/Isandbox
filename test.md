# lsandbox 测试文档

## 1. 测试目标

本文档用于验证 `lsandbox` Linux 命令行沙盒程序的核心功能是否符合预期。测试重点包括：

1. 基础启动功能。
2. Namespace 隔离功能。
3. OverlayFS 文件写入隔离功能。
4. cgroups v2 资源限制功能。
5. seccomp 系统调用过滤功能。
6. 网络访问与 DNS 可用性。
7. 下载与 `pip install` 隔离效果。
8. `--rm` 自动清理功能。
9. 主机环境不被污染。

测试默认在项目根目录执行：

```bash
cd ~/Isandbox
```

## 2. 测试环境

建议记录以下环境信息，便于报告和答辩说明：

```bash
uname -a
cat /etc/os-release
mount | grep cgroup
stat -fc %T /sys/fs/cgroup
./lsandbox --help 2>/dev/null || true
```

建议在测试前确认项目可以正常编译：

```bash
make clean
make
ls -lh ./lsandbox
```

预期结果：

- `make` 成功。
- 项目根目录下生成可执行文件 `lsandbox`。

## 3. 测试前清理

为避免残留目录影响结果，测试前建议清理旧沙盒目录。

```bash
sudo umount -l sandboxes/*/merged_tmp 2>/dev/null || true
sudo rm -rf sandboxes/test_* sandboxes/*box
mkdir -p sandboxes
```

如果你的项目没有 `merged_tmp` 目录，可以忽略 `umount` 报错。

## 4. 测试用例总览

| 编号 | 测试项 | 目标 |
|---|---|---|
| TC-01 | 基础命令启动 | 验证 `lsandbox run` 可以执行普通命令 |
| TC-02 | 交互式 Shell | 验证可以进入沙盒 Bash |
| TC-03 | UTS Namespace | 验证沙盒 hostname 隔离 |
| TC-04 | PID Namespace | 验证沙盒内进程视图隔离 |
| TC-05 | Mount Namespace 与 `/proc` | 验证沙盒内 `/proc` 重新挂载 |
| TC-06 | OverlayFS `/tmp` 写入隔离 | 验证沙盒写 `/tmp` 不污染主机 |
| TC-07 | `--rm` 自动清理 | 验证退出后删除沙盒写入层 |
| TC-08 | 保留写入层 | 验证不加 `--rm` 时可保留 upper 层 |
| TC-09 | 内存限制 | 验证 `--mem` 生效 |
| TC-10 | 进程数限制 | 验证 `--pids` 生效 |
| TC-11 | 网络访问 | 验证沙盒可访问 HTTPS 网站 |
| TC-12 | DNS 解析 | 验证域名解析可用且不破坏主机 DNS |
| TC-13 | 下载隔离 | 验证下载文件不进入主机 `/tmp` |
| TC-14 | pip 安装隔离 | 验证沙盒内安装 Python 包不污染主机 |
| TC-15 | seccomp 基础验证 | 验证默认 seccomp 不影响常用命令 |
| TC-16 | 异常退出清理 | 验证命令失败后清理逻辑正常 |

## 5. 详细测试用例

## TC-01 基础命令启动测试

### 测试命令

```bash
sudo ./lsandbox run --name test_basic --rm -- echo "hello lsandbox"
```

### 预期结果

输出：

```text
hello lsandbox
```

测试通过标准：

- 命令可以正常执行。
- 程序正常退出。
- 无明显权限错误、挂载错误或段错误。

## TC-02 交互式 Shell 测试

### 测试命令

```bash
sudo ./lsandbox run --name test_shell --rm -- bash
```

进入沙盒后执行：

```bash
whoami
pwd
exit
```

### 预期结果

- 可以进入 Bash。
- 可以执行普通命令。
- 输入 `exit` 后可以正常退出沙盒。

## TC-03 UTS Namespace 隔离测试

### 测试命令

主机执行：

```bash
hostname
```

启动沙盒：

```bash
sudo ./lsandbox run --name test_uts --rm -- bash
```

沙盒内执行：

```bash
hostname
exit
```

退出后主机再次执行：

```bash
hostname
```

### 预期结果

- 沙盒内 hostname 应显示为项目设置的沙盒主机名，例如 `lsandbox`。
- 主机 hostname 在沙盒退出前后保持不变。

## TC-04 PID Namespace 隔离测试

### 测试命令

```bash
sudo ./lsandbox run --name test_pid --rm -- bash
```

沙盒内执行：

```bash
ps aux
cat /proc/1/status | head
exit
```

### 预期结果

- `ps aux` 主要显示沙盒内进程。
- `/proc/1/status` 对应沙盒内的 1 号进程。
- 不应完整暴露主机所有进程。

## TC-05 Mount Namespace 与 /proc 测试

### 测试命令

```bash
sudo ./lsandbox run --name test_proc --rm -- bash
```

沙盒内执行：

```bash
mount | grep proc
ls /proc | head
ps aux
exit
```

### 预期结果

- 沙盒内 `/proc` 可用。
- `ps aux` 可以正常读取进程信息。
- `/proc` 应反映沙盒进程命名空间，而不是主机完整进程空间。

## TC-06 OverlayFS /tmp 写入隔离测试

### 测试命令

先在主机确认测试文件不存在：

```bash
rm -f /tmp/lsandbox_file_test.txt
ls /tmp/lsandbox_file_test.txt 2>/dev/null || echo "host clean"
```

进入沙盒：

```bash
sudo ./lsandbox run --name test_file --rm -- bash
```

沙盒内执行：

```bash
echo "sandbox write" > /tmp/lsandbox_file_test.txt
cat /tmp/lsandbox_file_test.txt
exit
```

退出后主机执行：

```bash
ls /tmp/lsandbox_file_test.txt 2>/dev/null || echo "not found on host"
```

### 预期结果

沙盒内可以看到：

```text
sandbox write
```

主机应输出：

```text
not found on host
```

测试通过标准：沙盒内写入 `/tmp` 不污染主机 `/tmp`。

## TC-07 --rm 自动清理测试

### 测试命令

```bash
sudo ./lsandbox run --name test_rm --rm -- bash -c 'echo hello > /tmp/rm_test.txt'
ls sandboxes/test_rm 2>/dev/null || echo "sandbox cleaned"
```

### 预期结果

输出：

```text
sandbox cleaned
```

测试通过标准：使用 `--rm` 后，沙盒退出时对应写入层目录被删除。

## TC-08 不加 --rm 保留写入层测试

### 测试命令

```bash
sudo ./lsandbox run --name test_keep -- bash -c 'echo keep > /tmp/keep_test.txt'
find sandboxes/test_keep -maxdepth 3 -type f 2>/dev/null | sort
```

### 预期结果

- `sandboxes/test_keep` 目录存在。
- 可以在写入层中找到 `keep_test.txt` 或对应写入内容。

测试结束后清理：

```bash
sudo rm -rf sandboxes/test_keep
```

## TC-09 内存限制测试

### 测试命令

启动一个较小内存限制的沙盒：

```bash
sudo ./lsandbox run --name test_mem --rm --mem 64M -- python3 - <<'PY'
a = []
try:
    while True:
        a.append(bytearray(10 * 1024 * 1024))
        print("allocated", len(a) * 10, "MB")
except MemoryError:
    print("MemoryError")
PY
```

### 预期结果

可能出现以下情况之一：

- Python 抛出 `MemoryError`。
- 进程被系统终止。
- 沙盒命令非 0 退出。

测试通过标准：程序不能无限制占用主机内存，资源限制能够阻止继续申请内存。

## TC-10 进程数限制测试

### 测试命令

```bash
sudo ./lsandbox run --name test_pids --rm --pids 32 -- bash -c '
count=0
while true; do
  sleep 60 &
  count=$((count+1))
  echo "started $count"
done
'
```

### 预期结果

- 创建到一定数量后，继续 `fork` 应失败。
- 沙盒不能无限制创建进程。

测试完成后如有残留进程，可执行：

```bash
sudo pkill -f "sleep 60" || true
```

## TC-11 网络访问测试

### 测试命令

```bash
sudo ./lsandbox run --name test_net --rm -- bash -c 'curl -I --max-time 10 https://pypi.org | head'
```

如果需要使用主机网络模式：

```bash
sudo ./lsandbox run --name test_net --rm --net host -- bash -c 'curl -I --max-time 10 https://pypi.org | head'
```

### 预期结果

输出中应包含类似内容：

```text
HTTP/2 200
```

测试通过标准：沙盒内可以访问 HTTPS 网站。

## TC-12 DNS 解析测试

### 测试命令

先记录主机 DNS 配置：

```bash
cp /etc/resolv.conf /tmp/resolv.before
cat /etc/resolv.conf
```

进入沙盒测试域名解析：

```bash
sudo ./lsandbox run --name test_dns --rm -- bash -c '
getent hosts pypi.org || true
python3 - <<"PY"
import socket
print(socket.gethostbyname("pypi.org"))
PY
'
```

退出后检查主机 DNS 配置是否变化：

```bash
diff -u /tmp/resolv.before /etc/resolv.conf || true
```

### 预期结果

- 沙盒内可以解析 `pypi.org`。
- 主机 `/etc/resolv.conf` 不应被沙盒修改。
- `diff` 没有差异，或只有系统网络管理器自身产生的正常变化。

测试通过标准：沙盒 DNS 可用，且不污染主机 DNS 配置。

## TC-13 下载隔离测试

### 测试命令

先清理主机测试文件：

```bash
rm -f /tmp/lsandbox_download_test.html
```

沙盒内下载：

```bash
sudo ./lsandbox run --name test_download --rm -- bash -c '
cd /tmp
curl -L --max-time 20 https://www.example.com -o lsandbox_download_test.html
ls -lh /tmp/lsandbox_download_test.html
'
```

退出后主机检查：

```bash
ls /tmp/lsandbox_download_test.html 2>/dev/null || echo "download not found on host"
```

### 预期结果

主机输出：

```text
download not found on host
```

测试通过标准：沙盒下载内容不会出现在主机 `/tmp`。

## TC-14 pip install 隔离测试

### 测试命令

```bash
sudo ./lsandbox run --name test_pip --rm --mem 2G --pids 256 -- bash -c '
python3 -m venv /tmp/venv
source /tmp/venv/bin/activate
python -m pip install --upgrade pip
python -m pip install requests
python - <<"PY"
import requests
r = requests.get("https://pypi.org", timeout=10)
print(r.status_code)
print(requests.__version__)
PY
'
```

退出后主机检查：

```bash
ls /tmp/venv 2>/dev/null || echo "venv not found on host"
```

### 预期结果

- 沙盒内输出 HTTP 状态码 `200`。
- 沙盒内输出 `requests` 版本号。
- 主机输出 `venv not found on host`。

测试通过标准：沙盒内可以安装和使用 Python 包，安装结果不污染主机 `/tmp`。

## TC-15 seccomp 基础测试

### 测试命令

默认 seccomp 模式下执行常用命令：

```bash
sudo ./lsandbox run --name test_seccomp --rm -- bash -c '
echo ok
ls /tmp >/dev/null
cat /proc/self/status | head >/dev/null
python3 -c "print(123)"
'
```

如果某程序疑似被 seccomp 拦截，对比关闭 seccomp：

```bash
sudo ./lsandbox run --name test_seccomp_off --rm --seccomp off -- bash -c '
python3 -c "print(123)"
'
```

### 预期结果

- 默认模式下常用命令正常执行。
- 如果关闭 seccomp 后程序才正常，说明需要检查 seccomp 规则是否过严。

## TC-16 异常退出清理测试

### 测试命令

```bash
sudo ./lsandbox run --name test_fail --rm -- bash -c 'echo before_fail > /tmp/fail.txt; exit 42'
echo $?
ls sandboxes/test_fail 2>/dev/null || echo "cleaned after failure"
```

### 预期结果

- `lsandbox` 应正确处理子进程异常退出。
- 使用 `--rm` 时，即使命令返回非 0，沙盒目录也应被清理。

## 6. 主机污染检查

完成所有测试后，执行以下命令检查主机环境是否被污染：

```bash
ls /tmp/lsandbox_file_test.txt 2>/dev/null || echo "file test clean"
ls /tmp/lsandbox_download_test.html 2>/dev/null || echo "download test clean"
ls /tmp/venv 2>/dev/null || echo "pip venv clean"
find sandboxes -maxdepth 1 -type d | sort
```

预期结果：

- 主机 `/tmp` 下不存在测试文件。
- 使用 `--rm` 的测试沙盒目录不存在。
- 未使用 `--rm` 的沙盒目录可以手动清理。

清理命令：

```bash
sudo rm -rf sandboxes/test_*
```

## 7. 推荐完整验收命令组

如果只想快速跑一组验收测试，可以按下面顺序执行。

```bash
# 1. 编译
make clean && make

# 2. 基础启动
sudo ./lsandbox run --name test_basic --rm -- echo "hello lsandbox"

# 3. Namespace 基础检查
sudo ./lsandbox run --name test_ns --rm -- bash -c 'hostname; ps aux | head; mount | grep proc | head'

# 4. 文件隔离
rm -f /tmp/lsandbox_file_test.txt
sudo ./lsandbox run --name test_file --rm -- bash -c 'echo sandbox > /tmp/lsandbox_file_test.txt; cat /tmp/lsandbox_file_test.txt'
ls /tmp/lsandbox_file_test.txt 2>/dev/null || echo "file isolation ok"

# 5. 网络访问
sudo ./lsandbox run --name test_net --rm -- bash -c 'curl -I --max-time 10 https://pypi.org | head'

# 6. 下载隔离
rm -f /tmp/lsandbox_download_test.html
sudo ./lsandbox run --name test_download --rm -- bash -c 'cd /tmp && curl -L --max-time 20 https://www.example.com -o lsandbox_download_test.html && ls -lh lsandbox_download_test.html'
ls /tmp/lsandbox_download_test.html 2>/dev/null || echo "download isolation ok"

# 7. pip 隔离
sudo ./lsandbox run --name test_pip --rm --mem 2G --pids 256 -- bash -c '
python3 -m venv /tmp/venv &&
source /tmp/venv/bin/activate &&
python -m pip install requests &&
python - <<"PY"
import requests
r = requests.get("https://pypi.org", timeout=10)
print(r.status_code)
print(requests.__version__)
PY
'
ls /tmp/venv 2>/dev/null || echo "pip isolation ok"
```

## 8. 测试结果记录表

| 编号 | 测试项 | 测试命令是否执行 | 实际结果 | 是否通过 | 备注 |
|---|---|---|---|---|---|
| TC-01 | 基础命令启动 |  |  |  |  |
| TC-02 | 交互式 Shell |  |  |  |  |
| TC-03 | UTS Namespace |  |  |  |  |
| TC-04 | PID Namespace |  |  |  |  |
| TC-05 | Mount Namespace 与 `/proc` |  |  |  |  |
| TC-06 | `/tmp` 写入隔离 |  |  |  |  |
| TC-07 | `--rm` 自动清理 |  |  |  |  |
| TC-08 | 保留写入层 |  |  |  |  |
| TC-09 | 内存限制 |  |  |  |  |
| TC-10 | 进程数限制 |  |  |  |  |
| TC-11 | 网络访问 |  |  |  |  |
| TC-12 | DNS 解析 |  |  |  |  |
| TC-13 | 下载隔离 |  |  |  |  |
| TC-14 | pip 安装隔离 |  |  |  |  |
| TC-15 | seccomp 基础验证 |  |  |  |  |
| TC-16 | 异常退出清理 |  |  |  |  |

## 9. 通过标准

本项目可以认为通过基础验收的条件如下：

1. `lsandbox run` 可以正常启动命令和交互式 Shell。
2. 沙盒内 hostname、进程视图、`/proc` 与主机形成隔离。
3. 沙盒内写入 `/tmp` 不会污染主机 `/tmp`。
4. 使用 `--rm` 后沙盒写入层可以自动清理。
5. `--mem` 可以限制内存占用。
6. `--pids` 可以限制进程创建数量。
7. 沙盒内可以完成基本网络访问和 DNS 解析。
8. 沙盒内下载和 `pip install` 产生的文件不会保留到主机 `/tmp`。
9. 默认 seccomp 不影响常用命令执行。
10. 异常退出时仍能正确执行清理逻辑。

## 10. 常见失败分析

### 10.1 `Permission denied`

可能原因：

- 残留沙盒目录属于 root。
- 上一次 OverlayFS 没有正确卸载。
- 普通用户直接删除 root 创建的目录失败。

处理方式：

```bash
sudo umount -l sandboxes/*/merged_tmp 2>/dev/null || true
sudo rm -rf sandboxes/test_*
```

### 10.2 DNS 解析失败

现象：

```text
Could not resolve host
Temporary failure in name resolution
```

排查顺序：

```bash
# 主机测试
curl -I https://pypi.org
cat /etc/resolv.conf

# 沙盒测试
sudo ./lsandbox run --name test_dns --rm -- bash -c 'cat /etc/resolv.conf; getent hosts pypi.org'
```

如果主机正常、沙盒失败，应检查沙盒是否正确读取或绑定 DNS 配置，以及是否误改了主机 `/etc/resolv.conf`。

### 10.3 pip 安装失败

可能原因：

- 网络不可用。
- DNS 解析失败。
- 内存限制过小。
- 进程数限制过小。
- seccomp 规则过严。

可使用以下命令对比：

```bash
sudo ./lsandbox run --name test_pip_debug --rm --seccomp off --mem 2G --pids 256 -- bash
```

### 10.4 `--rm` 后目录仍存在

可能原因：

- 沙盒退出时清理逻辑没有执行。
- OverlayFS 仍处于挂载状态。
- 子进程未完全退出。

排查：

```bash
mount | grep sandboxes
ps aux | grep lsandbox
```

手动清理：

```bash
sudo umount -l sandboxes/test_rm/merged_tmp 2>/dev/null || true
sudo rm -rf sandboxes/test_rm
```
