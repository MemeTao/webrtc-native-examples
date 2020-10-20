#include <iostream>
#include <chrono>
#include "api/video/i420_buffer.h"
#include "api/create_peerconnection_factory.h"

#include "api/audio_codecs/audio_decoder_factory.h"
#include "api/audio_codecs/audio_encoder_factory.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"

#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "api/video_codecs/builtin_video_encoder_factory.h"
#include "api/video_codecs/video_decoder_factory.h"
#include "api/video_codecs/video_encoder_factory.h"

#include "media/base/video_adapter.h"
#include "media/base/video_broadcaster.h"

#include "pc/video_track_source.h"
#include "i420_creator.h"


static auto g_signal_thread = rtc::Thread::CreateWithSocketServer();

static rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> g_peer_connection_factory = nullptr;

rtc::scoped_refptr<webrtc::PeerConnectionInterface> get_default_peer_connection(
        rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> factory,
        rtc::Thread* signaling_thread, webrtc::PeerConnectionObserver* observer)
{
    webrtc::PeerConnectionInterface::RTCConfiguration config;
    config.sdp_semantics = webrtc::SdpSemantics::kUnifiedPlan;
    config.enable_dtls_srtp = true;
    //webrtc::PeerConnectionInterface::IceServer server;
    //server.uri = GetPeerConnectionString();
    //config.servers.push_back(server);
    auto peer_connection = factory->CreatePeerConnection(
        config, nullptr, nullptr, observer);
    assert(peer_connection);
    return peer_connection;
}

static int64_t cur_time()
{
    int64_t time_cur = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    return time_cur;
}

class DummySetSessionDescriptionObserver
    : public webrtc::SetSessionDescriptionObserver {
 public:
  static DummySetSessionDescriptionObserver* Create() {
    return new rtc::RefCountedObject<DummySetSessionDescriptionObserver>();
  }
  virtual void OnSuccess() { }
  virtual void OnFailure(webrtc::RTCError error) { assert(false);}
};

class AudioReceiver : public webrtc::AudioTrackSinkInterface {
public:
	  virtual void OnData(const void* audio_data,
	                      int bits_per_sample,
	                      int sample_rate,
	                      size_t number_of_channels,
	                      size_t number_of_frames)
	  {
		  //we can handle audio data here;
	  }
};

class SimpleClient : public webrtc::PeerConnectionObserver,
    public webrtc::CreateSessionDescriptionObserver{
public:
    SimpleClient(bool sending) {
        if(!sending) {
            audio_receiver_ = std::make_shared<AudioReceiver>();
        }
    }
    ~SimpleClient() {
        if(peer_connection_) {
            peer_connection_->Close();
            peer_connection_ = nullptr;
        }
    }
    void bind_peerconnection(rtc::scoped_refptr<webrtc::PeerConnectionInterface> conn) {
        peer_connection_ = conn;
    }
    void set_other(SimpleClient* receiver) {
        other_ = receiver;
    }
    void start() {
        g_signal_thread->Invoke<void>(RTC_FROM_HERE, [this]()
        {
            peer_connection_->CreateOffer(
                this, webrtc::PeerConnectionInterface::RTCOfferAnswerOptions());
        });
    }
    void on_ice_candidate(const webrtc::IceCandidateInterface* candidate) {
        peer_connection_->AddIceCandidate(candidate);
    }
    void on_sdp(webrtc::SessionDescriptionInterface* desc) {
        peer_connection_->SetRemoteDescription(DummySetSessionDescriptionObserver::Create(), desc);
        if(desc->GetType() == webrtc::SdpType::kOffer) {
            peer_connection_->CreateAnswer(
                    this, webrtc::PeerConnectionInterface::RTCOfferAnswerOptions());
        }
    }
protected:
    void OnAddStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) override{
        std::cout<<"[info] on add stream, id:"<<stream->id()<<std::endl;
    }
    void OnAddTrack(rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver,
            const std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>>& streams) override {
        auto track = receiver->track().get();
        std::cout<<"[info] on add track,kind:"<<track->kind()<<std::endl;
        if(track->kind() == "audio") {
        	auto audio_track = static_cast<webrtc::AudioTrackInterface*>(track);
        	audio_track->AddSink(audio_receiver_.get());
        }
    }
    void OnIceCandidate(const webrtc::IceCandidateInterface* candidate) override {
        peer_connection_->AddIceCandidate(candidate);
        std::string candidate_str;
        candidate->ToString(&candidate_str);
        /* sending ice to remote */
        other_->on_ice_candidate(candidate);
    }
    //unused
    void OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState new_state) override {}
    void OnRenegotiationNeeded() override {}
    void OnDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel) override {}
    void OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState new_state) override {}
protected:
    virtual void OnSuccess(webrtc::SessionDescriptionInterface* desc) override {
        peer_connection_->SetLocalDescription(DummySetSessionDescriptionObserver::Create(), desc);

        std::string sdp_str;
        desc->ToString(&sdp_str);
        /* sending sdp to remote */
        auto sdp_cp = webrtc::CreateSessionDescription(desc->GetType(), sdp_str, nullptr);
        other_->on_sdp(sdp_cp.release());
    }
    virtual void OnFailure(webrtc::RTCError error) {
        std::cout<<"[error] err:"<<error.message()<<std::endl;
        assert(false);
    }

private:
    rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_ = nullptr;
    SimpleClient* other_ = nullptr;

    std::shared_ptr<AudioReceiver> audio_receiver_;
};

int main()
{
    g_signal_thread->Start();
    g_peer_connection_factory = webrtc::CreatePeerConnectionFactory(
                  nullptr /* network_thread */, nullptr /* worker_thread */,
                  g_signal_thread.get()/* signaling_thread */, nullptr /*default_adm*/,
                  webrtc::CreateBuiltinAudioEncoderFactory(),
                  webrtc::CreateBuiltinAudioDecoderFactory(),
                  webrtc::CreateBuiltinVideoEncoderFactory(),
                  webrtc::CreateBuiltinVideoDecoderFactory(),
                  nullptr /* audio_mixer */, nullptr /* audio_processing */);

    rtc::LogMessage::ConfigureLogging("none debug tstamp thread");

    rtc::scoped_refptr<SimpleClient> sender = new rtc::RefCountedObject<SimpleClient>(true);
    rtc::scoped_refptr<SimpleClient> receiver = new rtc::RefCountedObject<SimpleClient>(false);

    auto audio_track = g_peer_connection_factory->CreateAudioTrack("audio", nullptr);
    auto peer_connection1 = get_default_peer_connection(g_peer_connection_factory, g_signal_thread.get(), sender.get());
    peer_connection1->AddTrack(audio_track, {"stream1"});

    auto peer_connection2 = get_default_peer_connection(g_peer_connection_factory, g_signal_thread.get(), receiver.get());

    sender->bind_peerconnection(peer_connection1);
    receiver->bind_peerconnection(peer_connection2);

    sender->set_other(receiver.get());
    receiver->set_other(sender.get());

    peer_connection1 = nullptr;  //decrease ref
    peer_connection2 = nullptr;  //decrease ref

    sender->start();

    std::string console;
    while(console != "exit") {
        std::cin >> console;
    }

    g_signal_thread->Invoke<void>(RTC_FROM_HERE, [&sender, &receiver](){
        sender = nullptr;
        receiver = nullptr;
    });
    g_signal_thread->Stop();

    return 0;
}
