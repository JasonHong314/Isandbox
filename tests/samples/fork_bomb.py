#!/usr/bin/env python3

import os
import sys
import time

def fork_bomb():
    print(f"[+] 父进程 PID: {os.getpid()}")
    print("[+] 开始 fork bomb 测试...")
    print("[+] 按 Ctrl+C 停止")
    
    count = 0
    try:
        while True:
            pid = os.fork()
            if pid == 0:
                while True:
                    time.sleep(0.1)
            else:
                count += 1
                if count % 10 == 0:
                    print(f"[+] 已创建 {count} 个子进程")
                time.sleep(0.01)
                
    except KeyboardInterrupt:
        print(f"\n[-] 收到 Ctrl+C，已创建 {count} 个子进程")
        sys.exit(0)
    except OSError as e:
        print(f"\n[-] 错误：{e}")
        print(f"[-] 进程数限制生效，成功创建 {count} 个子进程")
        sys.exit(1)

if __name__ == "__main__":
    fork_bomb()
