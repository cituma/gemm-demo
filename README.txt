参考 https://jackwish.net/2019/gemm-optimization.html 及 https://github.com/flame/how-to-optimize-gemm 一步步优化矩阵乘

# 编译
linux/android上直接执行 ./build_all.sh
windows上新建build目录,命令行中进入build目录，执行 cmake -A x64 .. 然后使用vs编译运行 

# 查看cache size方法:
linux:
sudo dmidecode -t cache
windows:
任务管理器->性能, 右下角就是cache size

# 查看TLB size:
linux:
getconf -a | grep PAGESIZE		一般都是4096

android可执行程序可以adb push到/data/local/tmp/下执行
