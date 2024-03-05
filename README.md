# RealTimeTest
LinuxのPOSIXリアルタイム拡張・リアルタイム性能等を確認するためのプログラム群です。
各プログラムディレクトリ内の同名シェルスクリプト(ex: affinity.sh)でCPUシールドやプロセス優先度等の標準的な実行手順が記述してあり、基本的にこれらシェルスクリプト(ex: affinity.sh)や`make test`を実行することでテストを実施することを想定しています。

```
$ tar -xf RealTimeTest-X.XXXX.tar.gz
$ cd RealTimeTest
$ make
$ sudo make test
```

オプション解析はシェルスクリプトとプログラムそれぞれで行われます。プログラム側にオプションを渡したい場合は`./affinity -- -v`といった様に2つのハイフンの後にプログラム用のオプションを追加してください。

以下はプログラムの一覧です。

- affinity
- sched
- semaphore
- shm\_open
- shm
- sigqueue
- msgqueue
- aio
- rfile
- loop
- timer
- nanosleep
- tswitch
- pdl
- memtest
- disk\_test
- timer\_loop
- clock

## affinity
プロセスのCPU割り付け動作の確認を行うプログラムです。一般的なLinuxではsched\_setaffinity()、RedHawk Linuxではmpadvise()を用いて確認を行います。

```
Options:
    -s : Use sched_*() instead of mpadvise()
```

## sched
LinuxのスケジューリングAPIであるschedが使用可能であることを確認します。


## semaphore
POSIXセマフォが使用可能であることを確認します。

## shm\_open
POSIX共有メモリが使用可能であることを確認します。

```
Options:
    -d : Debug mode
```

## shm
POSIX共有メモリを使用してサブプロセスとデータのやり取りが可能であることを確認します。

## sigqueue
sigqueue()を使用してシグナルの送信、またPOSIXリアルタイムシグナルがキューイングされていることを確認します。

```
Options:
    -p : Pause between test
    -d : Debug mode
```

## msgqueue
POSIXメッセージキューが使用可能であることを確認します。

```
Options:
    -d : Debug mode
```

## aio
POSIX非同期I/O(AIO)が使用可能であることを確認します。

```
Options:
    -l : Leave temp file; don't unlink temp file
```

## rfile
ネイティブ32bit環境では取り扱うことの出来ない2GB以上の大きなファイルが使用可能であることを確認します。

```
Options:
    -l : Leave temp file; don't unlink temp file
    -d : Debug mode
```

## loop
いわゆるビジーループで周期実行を行い、実行周期のジッタを測定します。

## timer(RCIM使用可)
以下の2種類のテストを行います。

- POSIXタイマーを使用して周期実行を行い、実行周期のジッタを測定します。
- nanosleep()を使用して異なる期間スリープを行い、スリープ復帰の精度を測定します。

POSIXタイマー及びnanosleep()の代わりにRCIMタイマーを使用することが可能です。

```
Options:
    -r : Use RCIM
```

## nanosleep
nanosleep()を使用して異なる期間スリープを行い、スリープ復帰の精度を測定します。

以前のfuzz\_shiftテストはnanosleepテストに統合されました。
使用するには`-f`オプションを加えて実行してください。

```
Options:
    -f : Test fuzz_shift
    -F : Test fuzz_shift(verbose)
```

## tswitch(RCIM使用可)
2つのスレッドを交互に遷移することでコンテキストスイッチのレイテンシ及びジッタを測定します。

POSIXタイマーの代わりにRCIMタイマーを使用することが可能です。

```
Options:
    -r : Use RCIM
    -d : Debug mode
```

## pdl
BSDソケットを使用して2つのプロセス間で周期実行を行い、各処理のレイテンシ及びジッタを測定します。

```
              timer->t0[i]--------------+---------------+----------------+
send client->server        Send Time    |               |                |
                     t1[i]--------------+               |                |
 nanosleep (100 us)        nanosleep    | Response Time |                |
                     t2[i]--------------+               | Timer Interval |
recv client<-server        Receive Time |               |                |
                     t3[i]------------------------------+                |
                                                                         |
              timer->t0[i]-----------------------------------------------+
                       :

* Total Jitter = (Response Time - nanosleep) / 2
               = (Send Time + Receive Time) / 2
```

```
Options:
    -s      : Run in server mode
    -c addr : Run in client mode (default: 127.0.0.1)
    -l num  : Run with load process (exec FFT)
    -p port : Set port (default: 20000)
    -f freq : Set timer frequency (default: 1000 Hz)
    -i iter : Set test iteration (default: 1)
    -a      : Enable O_ASYNC to sockets
    -D      : Enable debug mode
```

## memtest
メモリの読み書き速度を測定します。速度は実装や設定、データ内容に左右されるため参考値になります。

## disk\_test
システムディスクの読み書き速度を測定します。速度は実装や設定、データ内容に左右されるため参考値になります。

```
Options
    -t iter : Max iterations [1000]
    -b size : Initial block size [1]
    -f name : File name [DISK_TEST]
    -s size : File size in MB [20]
    -d      : Use O_DIRECT
    -r      : Random access mode
    -c      : Show CPU time spent by this process
    -a info : File advisory information [normal]
              normal     : No further special treatment
              random     : Expect random page references
              sequential : Expect sequential page references
              willneed   : Will need these pages
              dontneed   : Don't need these pages
              noreuse    : Data will be accessed once
    -l      : Leave temp file; don't unlink tempfile
```

## timer\_loop(RCIM使用可)
POSIXタイマーを使用して周期実行を行い、実行周期のジッターを測定します。
実行中に`p`をタイプすると表示の一時停止、`q`をタイプすると実行を終了します。

```
Options:
    -r     : Use RCIM
    -S sec : Set test duration
```

## clock(RCIM使用可)
RCIMクロックとPOSIXクロック(clock_realtime)を周期実行で比較し、クロックのジッターを測定します。

RCIMがシステムに存在しない場合、`-p`オプションでclock_realtimeとclock_monotonicの値を表示することができます。

```
Options:
    -p     : Not use RCIM
    -c     : Client mode, not emit signal voluntary (RCIM only)
    -d     : Show diff and histogram (RCIM only)
    -S sec : Set test duration
```

# メモ: CPUシールドについて
Ubuntu22.04時点でプリインでないため非RedHawk環境ではisolcpusカーネルパラメータを使用しているが、csetコマンドでもCPUシールドは可能。

```
$ sudo cset shield -c 10-11 --kthread=on
cset: --> activating shielding:
cset: moving 1316 tasks from root into system cpuset...
[==================================================]%
cset: kthread shield activated, moving 84 tasks into system cpuset...
[==================================================]%
cset: **> 40 tasks are not movable, impossible to move
cset: "system" cpuset of CPUSPEC(0-9) with 1360 tasks running
cset: "user" cpuset of CPUSPEC(10-11) with 0 tasks running
$ sudo cset shield
cset: --> shielding system active with
cset: "system" cpuset of CPUSPEC(0-9) with 1360 tasks running
cset: "user" cpuset of CPUSPEC(10-11) with 0 tasks running
$ sudo cset shield -r
cset: --> deactivating/reseting shielding
cset: moving 0 tasks from "/user" user set to root set...
cset: moving 1360 tasks from "/system" system set to root set...
[==================================================]%
cset: deleting "/user" and "/system" sets
cset: done
```

参考: https://www.codeblueprint.co.uk/2019/10/08/isolcpus-is-deprecated-kinda.html

# メモ: POSIX AIOとLinux AIO(libaio)とio\_uring(liburing)
- POSIX\_AIO
  ユーザ空間でのスレッド生成で実装しているため性能面に問題がある
- Linux AIO(libaio)
  O\_DIRECTでないと非同期IOにならない
- io\_uring(liburing)
  io\_uringは5.1で採用・liburingは22.04以降で利用可能

