# lsandbox

Linux 命令行沙盒程序，提供进程隔离、文件系统隔离、资源限制等功能。

## 环境要求

### 推荐运行环境
- **操作系统**：Ubuntu 22.04 / Ubuntu 24.04
- **内核要求**：Linux 5.x 及以上，支持 cgroups v2、OverlayFS、Namespace、seccomp
- **运行方式**：建议在真实 Ubuntu、WSL2 Ubuntu 或 Ubuntu 虚拟机中运行
- **权限要求**：需要 sudo 权限启动沙盒

### 必须具备的 Linux 内核能力
- **Namespace**：Mount、PID、UTS、IPC、Network（可选）
- **OverlayFS**：用于 RootFS Overlay 文件系统隔离
- **cgroups v2**：memory.max、memory.swap.max、pids.max、cpu.max
- **seccomp-bpf**：用于系统调用过滤，依赖 libseccomp
- **chroot / mount / mknod**：构造沙盒根文件系统

### 编译依赖
```bash
sudo apt update
sudo apt install -y build-essential gcc make libseccomp-dev
```

### 常用测试工具依赖（推荐）
```bash
sudo apt install -y curl wget git python3 python3-pip vim nano procps iproute2
```

### cgroups v2 检查
```bash
# 检查是否使用 cgroups v2
stat -fc %T /sys/fs/cgroup

# 检查可用 controller
cat /sys/fs/cgroup/cgroup.controllers

# 如果 cgroup.subtree_control 没有启用 controller
sudo sh -c 'echo "+memory +pids +cpu" > /sys/fs/cgroup/cgroup.subtree_control'
```

### OverlayFS 检查
```bash
cat /proc/filesystems | grep overlay
```

## 编译
```bash
make clean
make
```

## 基本用法
```bash
sudo ./lsandbox run [选项] -- <命令>
```

本系统需要使用 mount namespace、OverlayFS、chroot、cgroups v2 和私有 /dev 挂载等特权操作，因此启动器需要通过 sudo 获取初始化权限。系统在完成沙盒环境构建后，会在 execvp 前调用 setgid/setuid 降权到原始 sudo 用户，从而实现“特权初始化、非特权执行”的安全模型。

## 选项说明
| 选项 | 说明 | 默认值 | 示例 |
|------|------|--------|------|
| `--name <名称>` | 指定沙盒名称 | `default` | `--name testbox` |
| `--rm` | 沙盒退出后自动清理写入层 | 不清理 | `--rm` |
| `--mem <大小>` | 限制内存使用 | `1G` | `--mem 512M`, `--mem 2G` |
| `--pids <数量>` | 限制进程数量 | `128` | `--pids 64`, `--pids 128` |
| `--cpu <百分比>` | 限制 CPU 使用率 | `100` | `--cpu 100` |
| `--seccomp <模式>` | 系统调用过滤模式：`off`, `basic`, `strict` | `basic` | `--seccomp basic`, `--seccomp off` |
| `--net <模式>` | 网络模式：`host`（使用主机网络）, `off`（禁用网络） | `host` | `--net host` |
| `--overlay-tmp` | 隔离 /tmp 目录 | 不隔离 | `--overlay-tmp` |
| `--overlay-workdir` | 隔离当前工作目录 | 不隔离 | `--overlay-workdir` |

## 常用命令

### 启动交互式沙盒
```bash
sudo ./lsandbox run --name testbox -- bash
```

### 启动后自动清理
```bash
sudo ./lsandbox run --name testbox --rm -- bash
```

### 运行单条命令
```bash
sudo ./lsandbox run --name cmdtest -- echo "hello lsandbox"
```

### 限制内存
```bash
sudo ./lsandbox run --name membox --mem 512M -- bash
sudo ./lsandbox run --name membox --mem 2G -- bash
```

### 限制进程数
```bash
sudo ./lsandbox run --name pidbox --pids 64 -- bash
```

### 同时设置资源限制
```bash
sudo ./lsandbox run --name limitbox --rm --mem 1G --pids 128 -- bash
```

### 关闭 seccomp（调试用）
```bash
sudo ./lsandbox run --name debugbox --rm --seccomp off -- bash
```

### 使用主机网络
```bash
sudo ./lsandbox run --name netbox --rm --net host -- bash
```

## 文件隔离验证
```bash
# 启动沙盒
sudo ./lsandbox run --name filebox --rm -- bash

# 沙盒内创建文件
echo "hello from sandbox" > /tmp/lsandbox_test.txt
cat /tmp/lsandbox_test.txt
exit

# 主机检查（应该不存在）
ls /tmp/lsandbox_test.txt
```

## Python 虚拟环境示例
```bash
# 启动沙盒
sudo ./lsandbox run --name pipbox --rm --mem 2G --pids 256 -- bash

# 沙盒内执行
python3 -m venv /tmp/venv
source /tmp/venv/bin/activate
python -m pip install --upgrade pip
python -m pip install requests
python - <<'PY'
import requests
r = requests.get("https://pypi.org", timeout=10)
print(r.status_code)
PY
exit
```

## 推荐默认配置
- **网络**：host
- **RootFS Overlay**：开启
- **cgroup**：开启
- **内存**：1G
- **swap**：0
- **进程数**：128
- **CPU**：100
- **seccomp**：basic
- **权限降级**：开启
- **--rm**：默认关闭

## 注意事项
1. 当前项目需要 sudo 启动，因为需要 mount、chroot、cgroup、mknod 等特权操作。
2. 项目采用"root 初始化，普通用户运行"的模式，避免目标程序长期以 root 身份执行。
3. 如果使用 RootFS Overlay，沙盒内对任意路径的写入应进入 `sandboxes/<name>/upper_root`，不会污染主机。
4. 如果使用 `--rm`，沙盒退出后 `sandboxes/<name>` 会被自动删除。
5. 如果 DNS 解析失败，需要确认沙盒 rootfs 内 `/etc/resolv.conf` 已正确生成。

## 查看帮助
```bash
./lsandbox --help
```
