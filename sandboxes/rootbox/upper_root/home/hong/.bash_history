ls
make clean
make qemu
git add .
git commit -m "3/4 tests"
make qemu
make grade
git add .
git commit -m "lab lock finished"
git push
git push origin
git fetch
git fetch upstream
git checkout net
make clean
tcpdump -XXnr packets.pcap
make qemu
make grade
killall qemu
make grade
make clean
make grade
git add .
git commit -m "lab network finished"
git push origin
git fetch upstream
git checkout cow
make clean
mkdir os_lab
cd os_lab
code .
ps -ejf HelloWorld.out
ps -aux
gcc HelloWorld.c -o HelloWorld.out
./HelloWorld.out
./HelloWorld.out
git checkout util
git fetch
git checkout util
make clean
make grade
make qemu
git fetch
git checkout lock
make clean
git fetch
git checkout lock
git add .
git commit -m "update time"
git checkout lock
make grade
git fetch upstream
git checkout traps
make clean
make fs.img
./bttest
bttest
make qemu
addr2line -e kernel/kernel0x0000000080001e9c
0x0000000080001d18
0x0000000080001a9c
make qemu
addr2line -e kernel/kernel 0x0000000080001e9c
0x0000000080001d18
0x0000000080001a9c
cd xv6_labs_2025
ls
cd x6-labs-2025
cd xv6-labs-2025
code .
make qemu
riscv64-unknown-elf-addr2line -e kernel/kernel 0x80001906
riscv64-linux-gnu-addr2line -e kernel/kernel 0x80001906
make qemu
make grade
git add .
git commit -m "lab trapsfinished"
git push origin
git fetch upstream
git checkout cow
make clean
make qemu
make grade
make qemu
make grade
killall qemu
killall qemu.real
killall qemu-system-riscv64
make grade
lsof -i :26000
kill -9 41501
kill -9 42474
lsof -i :26000
make grade
git add .
git branch
git commit -m "lab cow finished"
git push origin
git fetch upstream
git checkout fs
make clean
make qemu
git add .
git commit -m "large files finished"
make qemu
make grade
git add .
git branch
git commit -m "lab fs finished"
git push origin
git fetch upstream
git checkout pgtbl
make qemu
git add .
git commit -m "finished without superpage"
git push origin
git checkout mmap
make clean
git branch
git checkout pgtbl
git checkout -f pgtbl
make qemu
make clean
make qemu
make clean
make qemu
make grade
lsof -i :26000
kill -9 29391
kill -9 28495
lsof -i :26000
make qemu
make grade
git add .
git commit -m "lab pgtbl finished"
git push origin
conda activate odcs
pip config set global.index-url https://pypi.tuna.tsinghua.edu.cn/simple
ls
cd odcs_llm_project
conda create -n odcs python=3.10 -y
conda activate odcs
pip install -r requirements.txt
/home/hong/anaconda3/envs/odcs/bin/python /home/hong/odcs_llm_project/scripts/prepare_dolly.py
python train.py   --method sft   --max_steps 20   --max_seq_length 192   --output_dir outputs/sft_test
nvidia-smi
conda activate odcs
pip config set global.index-url https://pypi.tuna.tsinghua.edu.cn/simple
pip install -r requirements.txt
ls
cd odcs_llm_project
pip config set global.index-url https://pypi.tuna.tsinghua.edu.cn/simple
pip install -r requirements.txt
pip install torch torchvision torchaudio --index-url https://mirrors.tuna.tsinghua.edu.cn/pytorch/whl/cu121
pip install torch torchvision torchaudio --index-url https://download.pytorch.org/whl/cu121
pip install torch torchvision torchaudio --index-url https://mirrors.aliyun.com/pytorch-wheels/cu121
pip install torch torchvision torchaudio --index-url https://download.pytorch.org/whl/cu121
pip install -r requirements.txt
mkdir scripts
notepad scripts\prepare_dolly.py
nano scripts/prepare_dolly.py
code .
mkdir -p raw_data
wget -O raw_data/databricks-dolly-15k.jsonl https://hf-mirror.com/datasets/databricks/databricks-dolly-15k/resolve/main/databricks-dolly-15k.jsonl
HF_ENDPOINT=https://hf-mirror.com huggingface-cli download Qwen/Qwen2.5-0.5B-Instruct   --local-dir models/Qwen2.5-0.5B-Instruct   --local-dir-use-symlinks False
HF_ENDPOINT=https://hf-mirror.com huggingface-cli download Qwen/Qwen2.5-0.5B-Instruct   --local-dir models/Qwen2.5-0.5B-Instruct   --local-dir-use-symlinks False
python train.py   --base_model models/Qwen2.5-0.5B-Instruct   --method sft   --max_steps 20   --max_seq_length 192   --output_dir outputs/qwen_test
mkdir -p models
HF_ENDPOINT=https://hf-mirror.com hf download Qwen/Qwen2.5-0.5B-Instruct --local-dir models/Qwen2.5-0.5B-Instruct
python train.py   --base_model models/Qwen2.5-0.5B-Instruct   --method sft   --max_steps 20   --max_seq_length 192   --output_dir outputs/qwen_test
python train.py   --base_model models/Qwen2.5-0.5B-Instruct   --method odcs   --max_steps 20   --max_seq_length 192   --output_dir outputs/qwen_odcs_test
python eval.py --model_dir outputs/qwen_test --name qwen_sft_test
head -n 50 data/valid.jsonl > data/valid_50.jsonl
python eval.py   --model_dir outputs/qwen_test   --valid_file data/valid_50.jsonl   --name qwen_sft_test
python eval.py   --model_dir outputs/qwen_odcs_test   --valid_file data/valid_50.jsonl   --name qwen_odcs_test
rm -f results/eval_results.csv
head -n 200 data/valid.jsonl > data/valid_200.jsonl
python -c "import torch; print(torch.cuda.is_available()); print(torch.cuda.get_device_name(0) if torch.cuda.is_available() else 'NO CUDA')"
find . -name '*:Zone.Identifier' -delete
pip install datasets scikit-learn
conda activate odcs
pip install datasets scikit-learn
(base) hong@hjx:~/odcs_llm_project$ python train.py \
Traceback (most recent call last):
ModuleNotFoundError: No module named 'torch'
python train.py   --method sft   --max_steps 20   --max_seq_length 192   --output_dir outputs/sft_test
export HF_ENDPOINT=https://hf-mirror.com
python train.py --method sft --max_steps 20 --max_seq_length 192 --output_dir outputs/sft_test
git brunch
git branch
cd os_lab
code .
cd ..
ls
cd xv6-labs-2025
ls
code .
git branch
ls
cd sw
ls
rm sw
cd ..
rm sw
rmdir dw
rmdir sw
rmdir -r sw
rm -r sw
ls
rm -r os_lab
cd xv6-labs-2025
code .
git branch
git switch util
make grade
git branch
git switch syscall
make grade
git switch pgtbl
make grade
git branch
git switch traps
make grade
git switch cow
make grade
git branch
git switch lock
make grade
git switch util
git branch
git switch net
make grade
git
git switch fs
make grade
git switch syscall
git switch fs
git branch
git pull origin
git branch
git switch main
git switch syscall
make grade
git switch utils
git switch util
make qemu
ls
cd xv6-labs-2025
code .
ls
cd xv6-labs-2025
code .
git branch
git switch pgtbl
make clean
git switch pgtbl
git switch -f pgtbl
git branch
make grade
ls
cd xv6-labs-2025
code .
git add .
git commit -m "simplefy the structure"
git push
git clone git@github.com:JasonHong314/Isandbox.git
cd Isandbox
echo inside lsandbox
exit
make
./lsandbox run -- /bin/echo hello
./lsandbox run -- bash
exit
ls
git clone https://github.com/JasonHong314/Isandbox.git
git clone git@github.com:JasonHong314/Isandbox.git
ls
cd Isandbox
code .
cd ..
rm -r Isandbox
rm -rf Isandbox
ls
sudo apt update
sudo apt install -y openssh-server
sudo service ssh start
hostname -I
make clean
make
make clean
make
make clean
make
sudo ./lsandbox run --name box1 -- bash
cat /tmp/lsandbox_test.txt
sudo cat sandboxes/box1/upper_tmp/lsandbox_test.txt
make clean
make
sudo ./lsandbox run --name keepbox -- bash
主机 /tmp/keep.txt 不存在
sandboxes/keepbox/upper_tmp/keep.txt 存在
cat /tmp/keep.txt
sudo cat sandboxes/keepbox/upper_tmp/keep.txt
sudo ./lsandbox run --name rmbox --rm -- bash
ls sandboxes/rmbox
cat /sys/fs/cgroup/lsandbox_membox/memory.max
sudo ./lsandbox run --name limitbox --mem 64M --pids 32 --cpu 50 -- bash
make clean
make
sudo ./lsandbox run --name cleanbox --mem 64M -- bash
ls /sys/fs/cgroup/lsandbox_cleanbox
sudo ./lsandbox run --name keepcg --mem 64M --pids 32 --cpu 50 --keep-cgroup -- bash
67108864
32
50000 100000
cat /sys/fs/cgroup/lsandbox_keepcg/memory.max
cat /sys/fs/cgroup/lsandbox_keepcg/pids.max
cat /sys/fs/cgroup/lsandbox_keepcg/cpu.max
sudo rmdir /sys/fs/cgroup/lsandbox_keepcg
gcc tests/samples/memory_bomb.c -o tests/samples/memory_bomb
sudo ./lsandbox run --name membomb --mem 64M -- tests/samples/memory_bomb
echo "memory.max:"
cat /sys/fs/cgroup/lsandbox_membomb/memory.max
echo "cgroup.procs:"
cat /sys/fs/cgroup/lsandbox_membomb/cgroup.procs
echo "memory.current:"
cat /sys/fs/cgroup/lsandbox_membomb/memory.current
echo "all lsandbox cgroups:"
find /sys/fs/cgroup -maxdepth 1 -type d -name 'lsandbox_*' -print
echo "memory.max:"
cat /sys/fs/cgroup/lsandbox_membomb/memory.max
echo "cgroup.procs:"
cat /sys/fs/cgroup/lsandbox_membomb/cgroup.procs
echo "memory.current:"
cat /sys/fs/cgroup/lsandbox_membomb/memory.current
echo "all lsandbox cgroups:"
find /sys/fs/cgroup -maxdepth 1 -type d -name 'lsandbox_*' -print
cat /sys/fs/cgroup/lsandbox_membomb/memory.swap.current
cat /sys/fs/cgroup/lsandbox_membomb/memory.swap.max
cat /sys/fs/cgroup/lsandbox_membomb/memory.events
sudo apt update
sudo apt install -y libseccomp-dev
make
make clean
make
sudo ./lsandbox run --name secbox --seccomp -- /bin/echo hello
sudo ./lsandbox run --name membox --mem 64M -- bash
stat -fc %T /sys/fs/cgroup
mount | grep cgroup
cat /sys/fs/cgroup/cgroup.controllers
cat /sys/fs/cgroup/cgroup.subtree_control
ls -l /sys/fs/cgroup/lsandbox/membox/memory.max
sudo rm -rf /sys/fs/cgroup/lsandbox
sudo ./lsandbox run --name membox --mem 64M -- bash
sudo rm -rf /sys/fs/cgroup/lsandbox
sudo mkdir /sys/fs/cgroup/lsandbox
sudo sh -c 'echo "+memory +pids +cpu" > /sys/fs/cgroup/lsandbox/cgroup.subtree_control'
sudo mkdir /sys/fs/cgroup/lsandbox/testbox
ls /sys/fs/cgroup/lsandbox/testbox | grep -E "memory.max|pids.max|cpu.max"
sudo rm -rf /sys/fs/cgroup/lsandbox
sudo rm -rf /sys/fs/cgroup/lsandbox_membox
sudo find /sys/fs/cgroup/lsandbox* -depth -type d -exec rmdir {} \; 2>/dev/null
sudo sh -c 'echo "" > /sys/fs/cgroup/lsandbox/testbox/cgroup.procs' 2>/dev/null
make clean
make
sudo ./lsandbox run --name membox --mem 64M -- bash
sudo ./lsandbox run --name membomb --mem 64M --keep-cgroup -- tests/samples/memory_bomb
make clean
make
sudo pkill -9 memory_bomb 2>/dev/null
sudo rmdir /sys/fs/cgroup/lsandbox_membomb 2>/dev/null
sudo ./lsandbox run --name membomb --mem 64M --keep-cgroup -- tests/samples/memory_bomb
sudo ./lsandbox run --name secbox --seccomp -- bash
git add .
git commit -m "add seccomp syscall filtering"
git push
whoami
id
echo $HOME
exit
ping -c 1 8.8.8.8
curl -I https://pypi.org
sudo ./lsandbox run --name pipbox --rm -- bash
exit
python3 -m venv /tmp/venv
source /tmp/venv/bin/activate
python -m pip install requests
python - <<'PY'
import requests

r = requests.get("https://pypi.org", timeout=10)
print(r.status_code)
print(requests.__version__)
PY

exit
ls -ld /tmp
python3 -m venv /tmp/venv
source /tmp/venv/bin/activate
python3 -m pip install requests
python3 - <<'PY'
import requests
r = requests.get("https://pypi.org", timeout=10)
print(r.status_code)
print(requests.__version__)
PY

exit
python3 -m pip install --target /tmp/pydeps requests
PYTHONPATH=/tmp/pydeps python3 - <<'PY'
import requests
r = requests.get("https://pypi.org", timeout=10)
print(r.status_code)
print(requests.__version__)
PY

exit
export DISPLAY=:0
google-chrome   --no-sandbox   --incognito   --disable-extensions   --new-window   https://baidu.com
exit
apt update && apt install -y wget gnupg
wget -q -O - https://dl.google.com/linux/linux_signing_key.pub | apt-key add -
echo "deb [arch=amd64] http://dl.google.com/linux/chrome/deb/ stable main" > /etc/apt/sources.list.d/google-chrome.list
apt update && apt install -y google-chrome-stable
exit
exit
export DISPLAY=:0
google-chrome --no-sandbox --new-window https://www.baidu.com
echo $DISPLAY
ps aux | grep X
xauth list
export DISPLAY=:0
google-chrome --no-sandbox --new-window https://www.baidu.com
exit
export DISPLAY=:0
google-chrome --no-sandbox --new-window https://www.baidu.com
sudo ./lsandbox run --name webbox -- bash
python3 -m http.server 8080
exit
export DISPLAY=:0
google-chrome --no-sandbox --new-window https://www.baidu.com
exit
hostname -I | awk '{print $1}'
# 2. 在沙箱内启动一个 HTTP 服务（在一号终端）
sudo ./lsandbox run --name webbox -- bash
ls -la /tmp/.X11-unix/
echo "DISPLAY=$DISPLAY"
ls -la /tmp/.X11-unix/X0
sudo ./lsandbox run --name webbox -- bash
sudo ./lsandbox run --name webbox -- bash -c '
echo "=== DISPLAY ==="
echo DISPLAY=$DISPLAY
echo "=== X socket ==="
ls -la /tmp/.X11-unix/X0 2>&1
echo "=== 测试 xdpyinfo ==="
which xdpyinfo && xdpyinfo 2>&1 | head -5
echo "=== 测试 xauth ==="
xauth list 2>&1
'
export DISPLAY=:0
google-chrome --no-sandbox --new-window https://www.baidu.com
export DISPLAY=:99
Xvfb :99 -screen 0 1920x1080x24 &
sleep 1
google-chrome --no-sandbox --new-window https://www.baidu.com
exit
mkdir -p /tmp/chrome-profile /tmp/chrome-cache
google-chrome   --user-data-dir=/tmp/chrome-profile   --disk-cache-dir=/tmp/chrome-cache   --no-first-run   --disable-dev-shm-usage   https://www.baidu.com
exit
echo "DISPLAY=$DISPLAY"
echo "WAYLAND_DISPLAY=$WAYLAND_DISPLAY"
echo "XDG_RUNTIME_DIR=$XDG_RUNTIME_DIR"
mkdir -p /tmp/chrome-profile /tmp/chrome-cache
google-chrome   --ozone-platform=wayland   --user-data-dir=/tmp/chrome-profile   --disk-cache-dir=/tmp/chrome-cache   --no-first-run   --disable-dev-shm-usage   https://www.baidu.com
exit
curl -I https://pypi.org
python3 - <<'PY'
import urllib.request

url = "https://pypi.org"
resp = urllib.request.urlopen(url, timeout=10)
print("status =", resp.status)
print("url =", resp.geturl())
PY

exit
mkdir -p /tmp/downloads
curl -L https://pypi.org/robots.txt -o /tmp/downloads/robots.txt
ls -l /tmp/downloads
cat /tmp/downloads/robots.txt | head
exit
mkdir -p /tmp/downloads
curl -L https://pypi.org/robots.txt -o /tmp/downloads/robots.txt
ls -l /tmp/downloads/robots.txt
exit
echo "sandbox download" > ~/lsandbox_home_test.txt
exit
echo sandbox_tmp > /tmp/rootfs_test.txt
echo sandbox_home > /home/hong/rootfs_test.txt
exit
