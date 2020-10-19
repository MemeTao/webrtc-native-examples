## webrtc 视频流

本章介绍如何使用webrtc发送、接受视频流。

同样的，仍旧是在同一个进程中使用2个peerconnection来进行收发视频流。

为了不涉及"摄像头采集”等操作，同时也为了让示例代码能够全平台运行，我这里手动构造了红、橙、黄、绿、青、蓝、紫的RGB数据作为视频流的每一帧。

**注:实际上是I420格式的视频数据，为了不给新手造成更多的理解障碍，以下统称它为RGB数据**

### 视频编解码器工厂

上节说到，创建PeerConnection需要对应peerconnectionfactory(简称factory)。

factory是通过webrtc::CreatePeerconnctionFactory(params)这个函数来创建的，这个函数的参数众多。这里对它的第7和第8个参数，也就是VideoEncoderFactory和
VideoDecoderFactory作个简单的介绍。

当我们在peerconnection上创建一个视频流，webrtc就会向factory查询它所支持的视频编码器类型，并将所支持的编码器写入到sdp中。

并且当webrtc发送RGB数据时，webrtc就使用factory内部的编码器进行编码。

当webrtc接收到来自远端发送过来的"已编码"的视频帧后，webrtc就使用factory内部的解码器进行解码。

VideoEncoderFactory是webrtc规定的一个接口类，使用者可以自定义实现，或者使用webrtc提供的默认实现。

```C++
//省略部分非关键函数
class VideoEncoderFactory {
  // Returns a list of supported video formats in order of preference, to use
  // for signaling etc.
  virtual std::vector<SdpVideoFormat> GetSupportedFormats() const = 0;  //返回支持的编码器类型(SDP格式)

  // Creates a VideoEncoder for the specified format.
  virtual std::unique_ptr<VideoEncoder> CreateVideoEncoder(             //返回一个指定类型(协商完，双方都支持的)的视频编码器
      const SdpVideoFormat& format) = 0;
};
```

对于刚接触webrtc的新手来说，了解到这里就可以了，后面我会接着介绍如何实现自定义的编码器、解码器（硬件加速）。

### 向PeerConnection添加视频流

使用以下代码向peerconnection中添加一个"视频源(video source)"。
```C++
video_track = factory->CreateVideoTrack("video", video_souce);
peerconnection->AddTrack(video_track);
```

这个video source是一个抽象类，用户必须实现规定的虚函数。

```C++
template <typename VideoFrameT>
class VideoSourceInterface {
 public:
  virtual ~VideoSourceInterface() = default;

  virtual void AddOrUpdateSink(VideoSinkInterface<VideoFrameT>* sink,
                               const VideoSinkWants& wants) = 0;
  // RemoveSink must guarantee that at the time the method returns,
  // there is no current and no future calls to VideoSinkInterface::OnFrame.
  virtual void RemoveSink(VideoSinkInterface<VideoFrameT>* sink) = 0;
};
```
addxxxSink其实就是设置一个关键的回调对象，VideoSinkInterface:

```C++
template <typename VideoFrameT>
class VideoSinkInterface {
 public:
  virtual ~VideoSinkInterface() = default;

  virtual void OnFrame(const VideoFrameT& frame) = 0;

  // Should be called by the source when it discards the frame due to rate
  // limiting.
  virtual void OnDiscardedFrame() {}
};
```

当我们产生一帧视频数据时，只要通过设置近来的这个sink，使用其上的VideoSinkInterface::OnFrame(frame)通知webrtc，webrtc就会去编码、传输。

伪代码如下：
```C++
class VideoSourceMock : public rtc::VideoSourceInterface<webrtc::VideoFrame> {
public:
    void start(frame)
    {
        while(true) {
            std::this_thread::sleep_for(10ms);
            auto video_frame = get_frame();   //假设get_frame()可以返回一帧视频图像
            broadcaster_.OnFrame(video_frame);//投递给webrtc
        }
    }
private:
    void AddOrUpdateSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink,
                                 const rtc::VideoSinkWants& wants) override {
        broadcaster_.AddOrUpdateSink(sink, wants);
        (void) video_adapter_; //we willn't use adapter at this demo
    }
    void RemoveSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink) override {
        broadcaster_.RemoveSink(sink);
        (void) video_adapter_; //we willn't use adapter at this demo
    }
private:
    rtc::VideoBroadcaster broadcaster_;
    cricket::VideoAdapter video_adapter_;
};
```
在发送端，向peerconnection设置一个video track，并在video track上添加一个如上的video source，就已经可以将"视频帧"投递给webrtc，剩下的编码、传输等工作webrtc会自动完成。

### sdp的变化

当我们向一个peerconnection添加视频流后，在create offer的时候，sdp中就会多出来一项:
```shell
v=0
o=- 5204290053496113649 2 IN IP4 127.0.0.1
s=-
t=0 0
a=group:BUNDLE 0
a=msid-semantic: WMS stream1
m=video 9 UDP/TLS/RTP/SAVPF 96 97 98 99 100 101 127 120 125 119 124 107 108 109 123 118 122 117 114
c=IN IP4 0.0.0.0
a=rtcp:9 IN IP4 0.0.0.0
a=ice-ufrag:qE9e
a=ice-pwd:vrZjsfbZ8njxNamShhas60+L
a=ice-options:trickle
a=fingerprint:sha-256 59:BC:FB:DA:29:42:CB:FF:99:BD:BC:92:C1:3B:2A:D1:EC:6E:1A:2F:17:CB:87:56:B7:9B:A4:81:54:59:66:31
a=setup:actpass
a=mid:0
### 以下是跟视频格式相关的一些字段
a=extmap:1 urn:ietf:params:rtp-hdrext:toffset
a=extmap:2 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time
a=extmap:3 urn:3gpp:video-orientation
a=extmap:4 http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01
a=extmap:5 http://www.webrtc.org/experiments/rtp-hdrext/playout-delay
a=extmap:6 http://www.webrtc.org/experiments/rtp-hdrext/video-content-type
a=extmap:7 http://www.webrtc.org/experiments/rtp-hdrext/video-timing
a=extmap:8 http://www.webrtc.org/experiments/rtp-hdrext/color-space
a=extmap:9 urn:ietf:params:rtp-hdrext:sdes:mid
a=extmap:10 urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id
a=extmap:11 urn:ietf:params:rtp-hdrext:sdes:repaired-rtp-stream-id
a=sendrecv
a=msid:stream1 video
a=rtcp-mux
a=rtcp-rsize
### vp8
a=rtpmap:96 VP8/90000
a=rtcp-fb:96 goog-remb
a=rtcp-fb:96 transport-cc
a=rtcp-fb:96 ccm fir
a=rtcp-fb:96 nack
a=rtcp-fb:96 nack pli
a=rtpmap:97 rtx/90000
a=fmtp:97 apt=96
### vp9
a=rtpmap:98 VP9/90000
a=rtcp-fb:98 goog-remb
a=rtcp-fb:98 transport-cc
a=rtcp-fb:98 ccm fir
a=rtcp-fb:98 nack
a=rtcp-fb:98 nack pli
a=fmtp:98 profile-id=0
a=rtpmap:99 rtx/90000
a=fmtp:99 apt=98
a=rtpmap:100 VP9/90000
a=rtcp-fb:100 goog-remb
a=rtcp-fb:100 transport-cc
a=rtcp-fb:100 ccm fir
a=rtcp-fb:100 nack
a=rtcp-fb:100 nack pli
a=fmtp:100 profile-id=2
a=rtpmap:101 rtx/90000
a=fmtp:101 apt=100
### h264
a=rtpmap:127 H264/90000
a=rtcp-fb:127 goog-remb
a=rtcp-fb:127 transport-cc
a=rtcp-fb:127 ccm fir
a=rtcp-fb:127 nack
a=rtcp-fb:127 nack pli
a=fmtp:127 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42001f
a=rtpmap:120 rtx/90000
a=fmtp:120 apt=127
a=rtpmap:125 H264/90000
a=rtcp-fb:125 goog-remb
a=rtcp-fb:125 transport-cc
a=rtcp-fb:125 ccm fir
a=rtcp-fb:125 nack
a=rtcp-fb:125 nack pli
a=fmtp:125 level-asymmetry-allowed=1;packetization-mode=0;profile-level-id=42001f
a=rtpmap:119 rtx/90000
a=fmtp:119 apt=125
a=rtpmap:124 H264/90000
a=rtcp-fb:124 goog-remb
a=rtcp-fb:124 transport-cc
a=rtcp-fb:124 ccm fir
a=rtcp-fb:124 nack
a=rtcp-fb:124 nack pli
a=fmtp:124 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42e01f
a=rtpmap:107 rtx/90000
a=fmtp:107 apt=124
a=rtpmap:108 H264/90000
a=rtcp-fb:108 goog-remb
a=rtcp-fb:108 transport-cc
a=rtcp-fb:108 ccm fir
a=rtcp-fb:108 nack
a=rtcp-fb:108 nack pli
a=fmtp:108 level-asymmetry-allowed=1;packetization-mode=0;profile-level-id=42e01f
a=rtpmap:109 rtx/90000
a=fmtp:109 apt=108
a=rtpmap:123 AV1X/90000
a=rtcp-fb:123 goog-remb
a=rtcp-fb:123 transport-cc
a=rtcp-fb:123 ccm fir
a=rtcp-fb:123 nack
a=rtcp-fb:123 nack pli
a=rtpmap:118 rtx/90000
a=fmtp:118 apt=123
a=rtpmap:122 red/90000
a=rtpmap:117 rtx/90000
a=fmtp:117 apt=122
a=rtpmap:114 ulpfec/90000
a=ssrc-group:FID 3124827794 1259095527
a=ssrc:3124827794 cname:DDQzIip6txmbJUKJ
a=ssrc:3124827794 msid:stream1 video
a=ssrc:3124827794 mslabel:stream1
a=ssrc:3124827794 label:video
a=ssrc:1259095527 cname:DDQzIip6txmbJUKJ
a=ssrc:1259095527 msid:stream1 video
a=ssrc:1259095527 mslabel:stream1
a=ssrc:1259095527 label:video
```

同样的，我们仍然选择性忽略很多字段。只关注字段 "a=rtpmap:96 vp8/90000"，还有vp9，h264。

它表示发送方支持vp8、vp9以及h264编码(受到技术以外的约束，暂不支持h265)，这是webrtc默认支持的三种编码方式。

当接收端收到这个offer，需要根据自己的解码能力给发送方一个answer，answer中会写入这三种编码格式中接收方能支持的是哪一种。

如果三种都支持，发送方会选择answer中排在最前面的格式创建编码器。

在这个例子中，发送端和接收端是同样的webrtc代码，三种格式都能支持，所以创建的就是vp8编码器，"answer"就不列出来了。

### 传输视频流

上节DataChannel中，我详细介绍了sdp，candidate交互的流程，当这些流程都成功执行完后，通道自然就会建立。

这时候webrtc会自动帮我们打通编码器和"video sink"之间的通路，"broadcaster_.onframe()"中投递下去的视频帧会达到编码器，然后一层层的向下转，最终被分片成多个RTP格式的UDP（也可能是TCP）报文发送到网络。

这个过程具体的是如何执行的，后面的章节会介绍。源码分析总是比较枯味的，如何仅仅是使用webrtc的话，大可不必阅读。

### 接收视频帧

回到sdp交互的流程中。当接收端确认了发送端的offer，webrtc就会通过回调告诉使用者: 发送端创建了一个视频流。

```C++
    void OnAddStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) override{
        std::cout<<"[info] on add stream, id:"<<stream->id()<<std::endl;
    }
    //关键是这个函数
    void OnAddTrack(rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver,
            const std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>>& streams) override {
        auto track = receiver->track().get();
        if(track->kind() == "video" && video_receiver_) {
            auto cast_track = static_cast<webrtc::VideoTrackInterface*>(track);
            cast_track->AddOrUpdateSink(video_receiver_.get(), rtc::VideoSinkWants());
        }
    }
```

在"OnAddTrack"函数中，webrtc传递了一个receiver对象，在其上我们可以获取到一个"track(对应发送方那个)"。

如果我们希望接收这个视频流，那么我们就需要王这个"track"上添加一个"sink"(video_receiver_)，这个"sink"需要按照webrtc规定好的接口来自行实现：

```C++
class VideoStreamReceiver : public rtc::VideoSinkInterface<webrtc::VideoFrame> {
public:
    void OnFrame(const webrtc::VideoFrame& frame) override {
        //可以从frame这个对象中获取到二进制数据(RGB)，查看这个数据结构即可
    }
};
```

webrtc会把接收到的每一帧都通过这个接口告诉使用者。

如果我们将示例中的数据写入到一个文件，并用相应的视频软件打开，你就能看到"红橙黄绿青蓝紫"以30帧播放。

本示例不展示如何渲染。

### end

源码:https://github.com/MemeTao/webrtc-native-samples/blob/master/src/video-channel

本例中，为了让新手更好理解webrtc的视频流交互流程:

* 并没有抓屏、录屏，渲染等流程
* webrtc规定输入的视频数据格式为I420格式，我将其描述成了RGB。关于RGB和I420之间的格式转化，在本例中也有相应的代码，读者需要自行阅读相关的资料理解这段代码。
