#include <iostream>
#include <future>
#include "api/scoped_refptr.h"
#include "api/peer_connection_interface.h"
#include "api/create_peerconnection_factory.h"

#include "api/audio_codecs/audio_decoder_factory.h"
#include "api/audio_codecs/audio_encoder_factory.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"

#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "api/video_codecs/builtin_video_encoder_factory.h"
#include "api/video_codecs/video_decoder_factory.h"
#include "api/video_codecs/video_encoder_factory.h"

static auto g_signal_thread = rtc::Thread::CreateWithSocketServer();

rtc::scoped_refptr<webrtc::PeerConnectionInterface> get_peer_connection(webrtc::PeerConnectionObserver* observer)
{
    rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> peer_connection_factory =
            webrtc::CreatePeerConnectionFactory(
                  nullptr /* network_thread */, nullptr /* worker_thread */,
                  g_signal_thread.get()/* signaling_thread */, nullptr /* default_adm */,
                  webrtc::CreateBuiltinAudioEncoderFactory(),
                  webrtc::CreateBuiltinAudioDecoderFactory(),
                  webrtc::CreateBuiltinVideoEncoderFactory(),
                  webrtc::CreateBuiltinVideoDecoderFactory(), nullptr /* audio_mixer */,
                  nullptr /* audio_processing */);

    webrtc::PeerConnectionInterface::RTCConfiguration config;
    config.sdp_semantics = webrtc::SdpSemantics::kUnifiedPlan;
    config.enable_dtls_srtp = true;
    //webrtc::PeerConnectionInterface::IceServer server;
    //server.uri = GetPeerConnectionString();
    //config.servers.push_back(server);

    auto peer_connection = peer_connection_factory->CreatePeerConnection(
        config, nullptr, nullptr, observer);
    assert(peer_connection);
    return peer_connection;
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

class SimpleClient : public webrtc::DataChannelObserver,
    public webrtc::PeerConnectionObserver,
    public webrtc::CreateSessionDescriptionObserver{
public:
    SimpleClient(bool initiative)
        :initiative_(initiative),
         peer_connection_(get_peer_connection(this)),
         other_(nullptr)
    {
        if(initiative_) {
            webrtc::DataChannelInit data_channel_config;
            data_channel_config.ordered = true;
            data_channel_ = peer_connection_->CreateDataChannel("data label", &data_channel_config);
            data_channel_->RegisterObserver(this);
        }
    }
    ~SimpleClient()
    {
        peer_connection_->Close();
    }
    void start()
    {
        g_signal_thread->Invoke<void>(RTC_FROM_HERE, [this]()
        {
            peer_connection_->CreateOffer(
                this, webrtc::PeerConnectionInterface::RTCOfferAnswerOptions());
        });
    }
    void set_other(SimpleClient* other) {
        other_ = other;
    }
    void received_ice_candidate(const webrtc::IceCandidateInterface* candidate) {
        peer_connection_->AddIceCandidate(candidate);
    }
    void received_sdp(webrtc::SessionDescriptionInterface* desc) {
        peer_connection_->SetRemoteDescription(DummySetSessionDescriptionObserver::Create(), desc);
        if(desc->GetType() == webrtc::SdpType::kOffer) {
            peer_connection_->CreateAnswer(
                    this, webrtc::PeerConnectionInterface::RTCOfferAnswerOptions());
        }
    }
protected:
    void OnStateChange() override
    {
        assert(data_channel_);
        if(data_channel_->state() ==
                webrtc::DataChannelInterface::DataState::kOpen)
        {
            std::string mess = initiative_ ? "hello,I'm A" : "hello,I'm B";
            webrtc::DataBuffer buf(mess);
            if(!data_channel_->Send(buf)) {
                std::cout<<"[error] send message failed"<<std::endl;
            }
        }
    }

    void OnDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel) override
    {
        data_channel_ = data_channel;
        data_channel_->RegisterObserver(this);
    }
    //  A data buffer was successfully received.
    void OnMessage(const webrtc::DataBuffer& buffer) {
        std::cout<<"[info] mess:"<<std::string(buffer.data.data<char>(), buffer.data.size())<<std::endl;
    }
    void OnIceCandidate(const webrtc::IceCandidateInterface* candidate) override
    {
        peer_connection_->AddIceCandidate(candidate);
        std::string candidate_str;
        candidate->ToString(&candidate_str);
        /* sending ice candidate to remote...
         *
         * ----------------------------------------> 1 ms
         * ----------------------------------------> 2 ms
         * ....
         * ....
         * ----------------------------------------> 1/2 rtt
         * ok,remote client have received candidate now and deliver it webrtc
         */
        (void) candidate_str;
        other_->received_ice_candidate(candidate);
    }
    //unused
    void OnSignalingChange(
          webrtc::PeerConnectionInterface::SignalingState new_state) override {}
    void OnRenegotiationNeeded() override {}
    void OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState new_state) override {}
protected:
    virtual void OnSuccess(webrtc::SessionDescriptionInterface* desc) override {
        peer_connection_->SetLocalDescription(DummySetSessionDescriptionObserver::Create(), desc);

        std::string sdp_str;
        desc->ToString(&sdp_str);
        /* sending sdp to remote...
         *
         * ----------------------------------------> 1 ms
         * ----------------------------------------> 2 ms
         * ....
         * ....
         * ----------------------------------------> 1/2 rtt
         * ok,remote client have received sdp now and deliver it to webrtc
         */
        auto sdp_cp = webrtc::CreateSessionDescription(desc->GetType(), sdp_str, nullptr);
        other_->received_sdp(sdp_cp.release());
    }
    virtual void OnFailure(webrtc::RTCError error) {
        std::cout<<"[error] err:"<<error.message()<<std::endl;
        assert(false);
    }

private:
    bool initiative_ = false;
    rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel_;
    rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_;

    SimpleClient* other_;
};


int main()
{
    rtc::LogMessage::ConfigureLogging("none debug tstamp thread");
    g_signal_thread->Start();

    rtc::scoped_refptr<SimpleClient> a = new rtc::RefCountedObject<SimpleClient>(true);
    rtc::scoped_refptr<SimpleClient> b = new rtc::RefCountedObject<SimpleClient>(false);
    a->set_other(b.get());
    b->set_other(a.get());

    a->start();

    std::string console;
    while(console != "exit") {
        std::cin>>console;
    }

    g_signal_thread->Invoke<void>(RTC_FROM_HERE, [&a, &b](){
        a = nullptr;
        b = nullptr;
    });
    g_signal_thread->Stop();

    return 0;
}
