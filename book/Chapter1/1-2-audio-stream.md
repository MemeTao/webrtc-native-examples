## webrtc 音频流

本章介绍如何使用webrtc发送、接收音频流，本章涉及到的一些知识点在上一章介绍过，这里就不罗嗦了。

同样的，仍旧是在同一个进程中使用2个peerconnection来进行收发音频流。

### AudioEncoderFactory

同视频一样，这里就不多介绍了。


### 向PeerConnection添加音频流
使用以下代码向peerconnection中添加一个"音频源(audio source)"。

```C++
audio_track = factory->CreateVideoTrack("audio", audio_souce);
peerconnection->AddTrack(audio_track);
```

这个audio source是一个抽象类，用户必须实现规定的虚函数。

这里与vidoe source不同的是，我们一般使用webrtc提供的实现。

```C++
factory->CreateAudioSource(cricket::AudioOptions()); //返回默认的音频源
```

值得注意的是，这是个空实现。
```C++
// pc/local_audio_source.(h | cpp)
class LocalAudioSource : public Notifier<AudioSourceInterface> {
 public:
  // Creates an instance of LocalAudioSource.
  static rtc::scoped_refptr<LocalAudioSource> Create(
      const cricket::AudioOptions* audio_options);

  SourceState state() const override { return kLive; }
  bool remote() const override { return false; }

  const cricket::AudioOptions options() const override { return options_; }

  void AddSink(AudioTrackSinkInterface* sink) override {}
  void RemoveSink(AudioTrackSinkInterface* sink) override {}

 protected:
  LocalAudioSource() {}
  ~LocalAudioSource() override {}

 private:
  void Initialize(const cricket::AudioOptions* audio_options);

  cricket::AudioOptions options_;
};

rtc::scoped_refptr<LocalAudioSource> LocalAudioSource::Create(
    const cricket::AudioOptions* audio_options) {
  rtc::scoped_refptr<LocalAudioSource> source(
      new rtc::RefCountedObject<LocalAudioSource>());
  source->Initialize(audio_options);
  return source;
}

void LocalAudioSource::Initialize(const cricket::AudioOptions* audio_options) {
  if (!audio_options)
    return;

  options_ = *audio_options;
}
```

webrtc取视频数据和音频数据的逻辑不一致，视频数据通过添加的"video source"来产生。而音频数据是通过一个叫"audio device"的对象来产生。

这个对象通过CreatePeerConnectionFactory()的第三个参数指定，如果为nullptr，则由webrtc自己来指定:
```C++
// media/engine/webrtc_voice_engine.cc:298
#if defined(WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE)
  // No ADM supplied? Create a default one.
  if (!adm_) {
    adm_ = webrtc::AudioDeviceModule::Create(
        webrtc::AudioDeviceModule::kPlatformDefaultAudio, task_queue_factory_);
  }
#endif  // WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE
  RTC_CHECK(adm());
  webrtc::adm_helpers::Init(adm());
```
如果我们不提供adm，那么就需要打开这个**WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE**这个宏，
否则，RTC_CHECK(adm())就会断言。

当我们编译webrtc时，在gn参数列表中，可以看到，这个参数默认是开启的:
```shell
rtc_include_internal_audio_device
    Current value (from the default) = true
      From //webrtc.gni:243
```

在 module/audio_devices/目录下，可以看到"audio_device"类在各种平台上的实现。

在ubuntu20.04上，它的实现是基于"PulseAudio"。

当它们初始化完成后，就会自动产生的音频数据。

在本章，我们不介绍如何自定义实现这个"auido_device"。

读者可能有困惑，既然这个"local_audio_source"是个空实现，为什么需要这一步。在后续的源码分析章节中，我会介绍。

读者暂时只需要知道需要创建这么一个source即可。

### sdp的变化
```shell
o=- 2941351251118757007 2 IN IP4 127.0.0.1
s=-
t=0 0
a=group:BUNDLE 0
a=msid-semantic: WMS stream1
m=audio 9 UDP/TLS/RTP/SAVPF 111 103 104 9 102 0 8 106 105 13 110 112 113 126
c=IN IP4 0.0.0.0
a=rtcp:9 IN IP4 0.0.0.0
a=ice-ufrag:+ykV
a=ice-pwd:OJ6ZBZBNRdI++8rpZOmDz8cW
a=ice-options:trickle
a=fingerprint:sha-256 87:57:9E:14:EB:75:BE:48:05:A9:70:2D:BF:65:C5:81:29:F2:6E:00:44:7C:12:CE:A7:42:13:54:F7:EB:01:8D
a=setup:actpass
a=mid:0
a=extmap:1 urn:ietf:params:rtp-hdrext:ssrc-audio-level
a=extmap:2 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time
a=extmap:3 http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01
a=extmap:4 urn:ietf:params:rtp-hdrext:sdes:mid
a=extmap:5 urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id
a=extmap:6 urn:ietf:params:rtp-hdrext:sdes:repaired-rtp-stream-id
a=sendrecv
a=msid:stream1 audio
a=rtcp-mux
a=rtpmap:111 opus/48000/2
a=rtcp-fb:111 transport-cc
a=fmtp:111 minptime=10;useinbandfec=1
a=rtpmap:103 ISAC/16000
a=rtpmap:104 ISAC/32000
a=rtpmap:9 G722/8000
a=rtpmap:102 ILBC/8000
a=rtpmap:0 PCMU/8000
a=rtpmap:8 PCMA/8000
a=rtpmap:106 CN/32000
a=rtpmap:105 CN/16000
a=rtpmap:13 CN/8000
a=rtpmap:110 telephone-event/48000
a=rtpmap:112 telephone-event/32000
a=rtpmap:113 telephone-event/16000
a=rtpmap:126 telephone-event/8000
a=ssrc:4067769755 cname:NqeyNmZKjtdwqI6r
a=ssrc:4067769755 msid:stream1 audio
a=ssrc:4067769755 mslabel:stream1
a=ssrc:4067769755 label:audio
```

通过"a=rtpmap:111 opus/48000/2"我们看到webrtc默认的音频编码格式是opus。

### 发送音频数据

webrtc自己控制编码、发送。

### 接收音频数据

对于默认的实现来说，音频数据会由webrtc来录取，也由webrtc来播放。

但是如果我们需要在接收端处理这些数据的话，可以在接收端向"audio track"添加回调:

```C++
void OnAddTrack(rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver,
            const std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>>& streams) override 
{
    auto track = receiver->track().get();
    if(track->kind() == "audio") {
        auto audio_track = static_cast<webrtc::AudioTrackInterface*>(track);
        audio_track->AddSink(audio_receiver_.get());
    }
}

//audio_receiver_的实现如下:
class AudioReceiver : public webrtc::AudioTrackSinkInterface {
public:
	  virtual void OnData(const void* audio_data,
	                      int bits_per_sample,
	                      int sample_rate,
	                      size_t number_of_channels,
	                      size_t number_of_frames)
	  {
		  //we can get audio data here;
	  }
};
```

### end

看完本章相信读者还有很多的困惑:
* 默认的audio_device获取的是哪里的数据
* 如何添加自定的"audio_device"，从而能够发送我想要发送的音频数据
* 如何禁止webrtc接收端自动播放接收到的音频数据，我们自己控制什么时候播放这些数据。

我会在后续的章节一一介绍。

本章的源码地址: https://github.com/MemeTao/webrtc-native-samples/tree/master/src/audio-channle
