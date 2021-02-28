# webrtc-native-samples

samples that demonstrate how to use webrtc native code(**Still under development**).

And there is a supporting tutorial to help readers better understand the source code,but only the Chinese version.

### how to build webrtc

All examples based on [goole webrtc native code](https://webrtc.googlesource.com/src).

commit id: **cccd55094dbec7b8a0f7823ecf9c69d674200d87**.(As long as the api is the same, the others may be fine)

I think you can build libwebrtc.a youself.

#### RTTI

rtti is turned off by default, so please do not enable it when you build webrtc.

### build

get source code:

```shell
$ git clone https://github.com/MemeTao/webrtc-native-samples webrtc-samples
$ cd  webrtc-samples
$ git submodule init
$ git submodule sync
$ git submodule udpate
```

for example if we want to build 'data-channle':

```shell
$ move libwebrtc.a src/datachannel
$ cd src/datachannle
$ ls
libwebrtc.a  main.cpp
```
using following cmds to build:
```shell
$ g++ main.cpp libwebrtc.a  \
    -I ../../third_party/webrtc/  \
    -I ../../third_party/webrtc/third_party/abseil-cpp/ \
    -DWEBRTC_POSIX  \
    -lpthread -ldl
```
run:
```shell
$./a.out
 [info] mess:hello,I'm A
 [info] mess:hello,I'm B
``
