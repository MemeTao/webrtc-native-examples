## webrtc 视频流

本章介绍如何使用webrtc发送、接受视频流。

同样的，仍旧是在同一个进程中使用2个peerconnection来进行收发视频流。

为了不涉及"摄像头采集”等操作，同时也为了让示例代码能够全平台运行，我这里手动构造了红、橙、黄、绿、青、蓝、紫的RGB数据作为视频流的每一帧。


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
