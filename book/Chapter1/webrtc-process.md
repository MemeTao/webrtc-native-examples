## webrtc连接流程简介

假设有AB两个用户希望使用webrtc互连，由A主动发起建立连接的请求。为了描述方便，没有视音频流，仅建立一个数据通道，并用伪代码示例。

```c++
auto peer_connetion = webrtc::PeerConnectionInterface();
data_channel = peer_connection.CreateDataChannel("data");
peer_connection.CreateOffer(observer, options);
data_channel.RegisterObserver(observer);

class Observer {
public:
    //创建sdp后，回调该函数
    void OnSuccess(SessionDescriptionInterface* sdp) {
        peer_connection.SetLocalDescription(observer, sdp);
        signal_server.send_to("B", sdp); //通过信令服务器告知对端本地sdp
    }
    //datachannel的状态回调
    void OnStateChange() {
        if(data_channel.state() == DataState::kOpen) {
            DataBuffer buf("hello,I'm A!");
            data_channel->send(buf);
        }
    }
}
```

当CreateDataChannel()被调用的时候，peerconnection内部做了标记，标识本次连接会用到数据通道，并在CreateOffer()的执行过程中，在sdp中添加数据通道的信息。当这个sdp发给对端的时候，对端就知道该创建一个对应的datachannel。

对于远端B，伪代码如下:

```c++
auto peer_connetion = webrtc::PeerConnectionInterface();

//收到A发送的sdp回调
void OnRemoteSdp(webrtc::SessionDescriptionInterface* sdp) {
    peer_connection.SetRemoteDescription(observer, sdp);
    peer_connection.CreateAnswer(observer, options);
} 

class Observer {
public:
    void OnSuccess(SessionDescriptionInterface* sdp) {
        peer_connection.SetLocalDescription(observer, sdp);
        signal_server.send_to("A", sdp); //通过信令服务器告知对端本地sdp
    }
    //与A的数据通道opened之后的回调
    void OnDataChannel(rtc::scoped_refptr<DataChannelInterface> data_channel) {
        data_channel_ = data_channel;
        data_channel_->RegisterObserver(this);
    }
    void OnStateChange() {
        if(data_channel_->state() == DataState::kOpen) {
            DataBuffer buf("hello,I'm B!");
            data_channel->send(buf);
        }
    }
private:
    rtc::scoped_refptr<DataChannelInterface> data_channel_ = nullptr;
}
```

在应用层调用SetLocalDescription()时，webrtc便开始收集"**候选地址**"。

#### candidate(候选地址)

局域网下的主机A，本地具有2个网络设备：以太网和802.11(wifi)，ip分别是192.168.1.2\10.133.1.2，并且它的公网地址是200.100.50.1。那么理论上该主机A有拥有三个udp candidate，分别就是上述的三个地址。将一个candidate打印成字符串，有如下信息：

``` shell
"candidate":"candidate:1779684516 1 udp 2122260223 192.168.29.185 56370 typ host
generation 0 ufrag 7XGL network-id 1","sdpClientType":"cpp",
"sdpMLineIndex":0,"sdpMid":"0","type":"candidates"
```

type=host标识这是个本地candidate，ip是192.168.29.185。

每收集到一个candidate，webrtc会回调OnIceCandidate，这时候应用层就需要将这个candidate发送给对端:

```c++
class Observer {
public:
    void OnIceCandidate(const webrtc::IceCandidateInterface* candidate) {
        signal_server.send_to("B", candidate); //通过信令服务器告知对端我们的candidate
    }
```

当某一端收到来自对端的candidate之后，需要将这个candidate告诉webrtc:

```c++
auto peer_connetion = webrtc::PeerConnectionInterface();
class B {
    void on_recv_candidate(const webrtc::IceCandidateInterface* candidate) {
        peer_connection.AddIceCandidate(candidate);
    }
}
```

紧接着，webrtc会将本地candidate和远端candidate做一个匹配，创建虚拟的连接(**connection**)。

#### connection

某一个局域网下的主机A只有一个网络设备，本地ip是192.168.1.2，它的外网地址是123.1.1.2。

另一个局域网下的主机B只有一个网络设备，本地ip是10.133.1.2，它的外网地址是200.1.1.2。

当B的2个candidate(本地和外网)发给A之后，A就会创建如下的**connection**:

* 192.168.1.1 <-> 10.133.1.2
* 192.168.1.1 <-> 200.1.1.2

**connection** 是一个虚拟的”连接”，作为一个可能可以连通的候选。webrtc会对所有connection进行连通性检测，类似于"ping"，能"ping"通则说明网络能通，并对所有的"connection"进行排序，选择出最好的那个connection(rtt最短)，并用于最后的流媒体传输。

在这之后，dtls握手就会进行，再紧接着，用户态的sctp协议握手进行，数据通道开始交互数据。

以上便是一个正常的webrtc连接的过程。

**完整源码**

https://github.com/MemeTao/webrtc-native-samples/blob/master/src/datachannel/main.cpp
