# lsandbox 项目技术文档

---

## 一、软件开发环境

| 类别 | 内容 |
|------|------|
| **操作系统** | Ubuntu 22.04 / Ubuntu 24.04 / WSL2 Ubuntu |
| **内核要求** | Linux 5.x 及以上，支持 cgroups v2、OverlayFS、Namespace、seccomp |
| **语言/工具** | C 语言、GCC、Make、libseccomp-dev |
| **权限要求** | 需要 sudo 权限启动沙盒 |

---

## 二、主要数据结构

### 2.1 沙箱配置结构体

| 字段名 | 类型 | 说明 |
|--------|------|------|
| `name` | `char[LSANDBOX_NAME_MAX]` | 沙盒名称 |
| `sandbox_dir` | `char[LSANDBOX_PATH_MAX]` | 沙盒工作目录路径 |
| `upper_root_dir` | `char[LSANDBOX_PATH_MAX]` | RootFS Overlay upper 层路径 |
| `work_root_dir` | `char[LSANDBOX_PATH_MAX]` | RootFS Overlay work 层路径 |
| `merged_root_dir` | `char[LSANDBOX_PATH_MAX]` | RootFS Overlay merged 层路径 |
| `memory_limit` | `char[LSANDBOX_MEMORY_MAX]` | 内存限制（如 "1G"） |
| `pids_limit` | `int` | 进程数限制 |
| `cpu_percent` | `int` | CPU 使用率限制百分比 |
| `seccomp_mode` | `lsandbox_seccomp_mode_t` | seccomp 过滤模式 |
| `enable_root_overlay` | `int` | 是否启用 RootFS Overlay |
| `enable_net` | `int` | 是否使用主机网络 |
| `enable_cgroup` | `int` | 是否启用 cgroup 限制 |
| `remove_after_exit` | `int` | 是否退出后自动删除 |

### 2.2 seccomp 模式枚举

| 枚举值 | 值 | 说明 |
|--------|----|------|
| `LSANDBOX_SECCOMP_OFF` | 0 | 关闭 seccomp 过滤 |
| `LSANDBOX_SECCOMP_BASIC` | 1 | 基础过滤模式（禁止高危 syscall） |
| `LSANDBOX_SECCOMP_STRICT` | 2 | 严格过滤模式（仅允许白名单 syscall） |

### 2.3 命名空间配置

| 字段名 | 说明 | 默认值 |
|--------|------|--------|
| `enable_pid_ns` | PID 命名空间 | 启用 |
| `enable_mount_ns` | Mount 命名空间 | 启用 |
| `enable_uts_ns` | UTS 命名空间 | 启用 |
| `enable_ipc_ns` | IPC 命名空间 | 启用 |
| `enable_user_ns` | User 命名空间 | 禁用 |
| `enable_net` | 网络命名空间（0=新建，1=主机） | 主机网络 |

---

## 三、界面形式

- **A. 命令行** ✓
- **B. 命令菜单** 
- **C. 可视化图形界面** 
- **D. 其它** 

> 项目采用纯命令行界面，通过 `lsandbox run` 命令启动沙箱。

---

## 四、基本功能实现情况

### 4.1 沙箱核心功能

| 功能 | 说明 | 状态 |
|------|------|------|
| (1) **命名空间隔离** | PID、Mount、UTS、IPC 命名空间 | ✓ |
| (2) **RootFS Overlay** | 根文件系统隔离，写入重定向到 upper 层 | ✓ |
| (3) **cgroup 资源限制** | 内存、swap、进程数、CPU 限制 | ✓ |
| (4) **seccomp 过滤** | 系统调用白名单/黑名单 | ✓ |
| (5) **权限降级** | 初始化后降权到普通用户 | ✓ |
| (6) **网络控制** | 支持 host/off 两种模式 | ✓ |
| (7) **DNS 配置** | 沙箱内自动配置 resolv.conf | ✓ |

### 4.2 运行时文件系统

| 功能 | 说明 | 状态 |
|------|------|------|
| (8) **私有 /dev** | tmpfs 挂载，创建设备文件 | ✓ |
| (9) **/proc 挂载** | 挂载 proc 文件系统 | ✓ |
| (10) **/run 挂载** | tmpfs 挂载运行时目录 | ✓ |
| (11) **/dev/shm** | 共享内存目录 | ✓ |
| (12) **/dev/pts** | 伪终端支持 | ✓ |

### 4.3 沙箱管理功能

| 功能 | 说明 | 状态 |
|------|------|------|
| (13) **沙箱创建** | `lsandbox run` | ✓ |
| (14) **自动清理** | `--rm` 参数支持 | ✓ |
| (15) **日志记录** | 启动、退出、信号日志 | ✓ |
| (16) **帮助信息** | `--help` 参数 | ✓ |

---

## 五、新增功能

| 序号 | 功能名称 | 说明 |
|------|----------|------|
| (17) | **进程数限制** | 通过 cgroup pids.max 防止 fork bomb |
| (18) | **内存限制** | 通过 cgroup memory.max 限制内存使用 |
| (19) | **CPU 限制** | 通过 cgroup cpu.max 限制 CPU 使用率 |
| (20) | **RootFS 完整隔离** | 整个根文件系统 overlay，写入不污染主机 |
| (21) | **DNS 智能配置** | 自动处理主机 DNS 配置，支持独立网络命名空间 |
| (22) | **权限安全** | 采用"特权初始化，普通用户运行"模式 |

---

## 六、核心模块文件结构

```
lsandbox/
├── include/
│   ├── sandbox.h       # 沙箱配置结构体定义
│   ├── namespace.h     # 命名空间相关接口
│   ├── rootfs.h        # RootFS Overlay 接口
│   ├── cgroup.h        # cgroup 资源限制接口
│   ├── seccomp_filter.h # seccomp 过滤接口
│   └── log.h           # 日志接口
├── src/
│   ├── main.c          # 命令行入口
│   ├── sandbox.c       # 沙箱配置与初始化
│   ├── namespace.c     # 命名空间创建与管理
│   ├── rootfs.c        # RootFS Overlay 实现
│   ├── cgroup.c        # cgroup 资源限制实现
│   ├── seccomp_filter.c # seccomp 过滤实现
│   ├── log.c           # 日志记录实现
│   └── utils.c         # 工具函数
└── tests/
    └── samples/        # 测试样例程序
```

---

## 七、技术特点

| 特点 | 说明 |
|------|------|
| **隔离性** | 完整的文件系统、进程、网络隔离 |
| **安全性** | seccomp 过滤 + 权限降级，防止权限逃逸 |
| **资源可控** | 精确的内存、CPU、进程数限制 |
| **环境一致性** | 继承主机文件系统，写入隔离 |
| **易用性** | 简洁的命令行接口，合理的默认配置 |
