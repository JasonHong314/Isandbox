# lsandbox

Linux 命令行沙盒程序，提供进程隔离、文件系统隔离、资源限制等功能。

## 编译

```bash
make
```

## 基本用法

```bash
sudo ./lsandbox run [选项] -- <命令>
```
由于本系统需要使用 mount namespace、OverlayFS、chroot、cgroups v2 和私有 /dev 挂载等特权操作，因此启动器需要通过 sudo 获取初始化权限。为避免目标程序以 root 身份运行，系统在完成沙盒环境构建后，会在 execvp 前调用 setgid/setuid 降权到原始 sudo 用户，从而实现“特权初始化、非特权执行”的安全模型。
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
## 查看帮助

```bash
./lsandbox --help
```
