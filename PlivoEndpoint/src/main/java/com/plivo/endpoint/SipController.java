

package com.plivo.endpoint;

import static org.webrtc.PeerConnection.ContinualGatheringPolicy.GATHER_ONCE;

import android.annotation.SuppressLint;
import android.content.Context;
import android.media.AudioManager;
import android.util.Log;

import org.json.JSONException;
import org.json.JSONObject;
import org.webrtc.AudioSource;
import org.webrtc.AudioTrack;
import org.webrtc.IceCandidate;
import org.webrtc.MediaConstraints;
import org.webrtc.MediaStream;
import org.webrtc.PeerConnection;
import org.webrtc.PeerConnectionFactory;
import org.webrtc.RtpSender;
import org.webrtc.SessionDescription;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;
import java.util.Objects;

public class SipController {

    public enum RegistrationEvent {
        REGISTERED,
        NOT_REGISTERED
    }

    public enum CallEvent {
        INCOMING_CALL,
        TERMINATE_CALL,
        CALL_ACCEPTED
    }

    public enum TerminatedReason{
        Error,
        Timeout,
        Replaced,
        LocalBye,
        RemoteBye,
        LocalCancel,
        RemoteCancel,
        Rejected,
        Referred
    }

    public enum CallState{
        EarlyMedia,
        Calling,
        CallAccepted,
        Connecting
    }


    public enum ErrorType {
        WEBRTC_ERROR,
        SIP_CONNECTION_ERROR,
        SIP_SESSION_ERROR
    }


    PeerConnection pc;
    PeerConnectionFactory peerConnectionFactory;
    Context context;
    AudioTrack localAudioTrack;
    Boolean isIceGatheringCompleted =  false;
    Boolean called = true;
    SipController sipController;
    MediaConstraints sdpConstraints;
    MediaStream stream;
    static PeerConnection temp= null;
    AudioManager audioManager ;
    Boolean didSentInvite = false;
    String xcallid;
    Boolean isIncomingCall = false;
    SignallingStats signallingStats;
    CallInsights callInsights;

    String username;
    String password;
    EventListener eventListener;
    Incoming incoming;

    private static SipController controller;

    public static SipController getInstance(EventListener eventListener) {
        if (controller == null) {
            controller = new SipController(eventListener);
        }
        return controller;
    }

    private SipController(EventListener eventListener){
        this.eventListener = eventListener;
        registerListners();

    }

    private void registerListners(){

        this.registerOnRegistrationEventListener(new SipController.OnRegistrationEventListener() {
            @Override
            public void onRegistrationEvent(SipController.RegistrationEvent event, String user) {
                System.out.println("rahool");
                System.out.println(event.name());
                eventListener.onLogin();
            }
        });

        this.registerOnCallEventListener(new SipController.OnCallEventListener() {
            @Override
            public void onCallEvent(SipController.CallEvent event, String user) {
                if (event== SipController.CallEvent.TERMINATE_CALL){
                    System.out.println("call hanged up");
                    eventListener.onIncomingCallHangup(incoming);
                    sipController.hangup();
                }
                if (event== SipController.CallEvent.INCOMING_CALL){
                    System.out.println("incoming call");
                    incoming = new Incoming("", "", "", "", controller);
                    eventListener.onIncomingCall(incoming);
                }
                if(event == SipController.CallEvent.CALL_ACCEPTED){
                    System.out.println("call accepted");
                }
            }
        });
    }

    public  interface OnRegistrationEventListener {
        void onRegistrationEvent(RegistrationEvent event, String user);
    }

    public  interface OnCallEventListener {
        void onCallEvent(CallEvent event, String user);
    }

    public  interface OnLogEventListener {
        void onLogEvent(String log);
    }

    public  interface OnErrorEventListener {
        void onErrorEvent(ErrorType type, String message);
    }

    private static final String TAG = "SipController";

    static {
        try {
            System.loadLibrary("rtcsip_jni");
        } catch (UnsatisfiedLinkError exc) {
            Log.e(TAG, "rtcsip_jni library not found");
        }
    }

    private OnRegistrationEventListener onRegistrationEventListener;
    private OnCallEventListener onCallEventListener;
    private OnLogEventListener onLogEventListener;
    private OnErrorEventListener onErrorEventListener;

    public synchronized  void setRemoteSDP(String remoteSDP){
        System.out.printf("set remote sdp \n %s", remoteSDP);
        SessionDescription.Type type = SessionDescription.Type.ANSWER;
        if (pc == null){
            createLocalPeerConnection();
            type = SessionDescription.Type.OFFER;
        }

        SessionDescription sessionDescription = new SessionDescription(type,remoteSDP);
        //temp.setRemoteDescription(new CustomSdpObserver("localSetRemoteDesc"), sessionDescription);
        pc.setRemoteDescription(new CustomSdpObserver("localSetRemoteDesc"), sessionDescription);

        System.out.println(pc.signalingState());
        //temp.getLocalDescription().description
    }

    String getXCallUUID(String callInfo){
        String id = null;
        String[] parts = callInfo.split(",");
        for(String s : parts){
            String[] info = s.split(":");
            if(info[0].equals(Global.xCallUUID)){
                id = info[1];
                break;
            }
        }
        return id;
    }

    public String getXcallid() {
        return xcallid;
    }

    public void sendDigits(String digit){
        if (digit.length() > 24) {
            System.out.println("Error in sending digits: digits length cannot be more than 24");
            return;
        }
        RtpSender m_audioSender = null;
        for (RtpSender sender : pc.getSenders()) {
            if (sender.track() != null) {
                String trackType = Objects.requireNonNull(sender.track()).kind();
                if (trackType.equals("audio")) {
                    Log.d(TAG, "Found audio sender.");
                    m_audioSender = sender;
                }
            }
        }
        if(m_audioSender != null){
            boolean isPlayed = Objects.requireNonNull(m_audioSender.dtmf()).insertDtmf(digit, 100, 500);
        }
    }

    public synchronized void setCallInfo(String callInfo){
        xcallid = getXCallUUID(callInfo);
    }

    public synchronized void setCallState(String state, int statusCode){
        CallState callState;
        try{
            callState = CallState.valueOf(state);
        } catch (IllegalArgumentException e){
            callState = null;
        }

        TerminatedReason terminateReason;
        try{
            terminateReason = TerminatedReason.valueOf(state);
        } catch (IllegalArgumentException e){
            terminateReason = null;
        }

        if(isIncomingCall){
            incomingCallStates(callState, statusCode, terminateReason);
        } else {
            outgoingCallStates(callState, statusCode, terminateReason);
        }
    }

    @SuppressLint("NewApi")
    public void outgoingCallStates(CallState callState, int statusCode, TerminatedReason terminateReason){
        if(callState != null){
            if(callState == CallState.Calling){
                signallingStats = new SignallingStats();
            }

            if(statusCode >= 180 && statusCode <= 183){
                signallingStats.setRingStartTime();
                signallingStats.setPostDialDelay();
            }

            if(callState == CallState.CallAccepted){
                if(signallingStats.getPostDialDelay() == null){
                    signallingStats.setPostDialDelay();
                }
                signallingStats.setAnswerTime();
                signallingStats.setCallConfirmedTime();
                signallingStats.setXCallUUID(xcallid);
                // send call answerstats
                callInsights.sendAnswerEvent(signallingStats, false);
                callInsights.initRTPStats();
                callInsights.sendRtpStats(signallingStats, pc);
            }
        }

        if(terminateReason != null){
            if(statusCode >= 480 && statusCode<= 489){
                signallingStats.setHangupTime();
                if(statusCode == 486) {
                    signallingStats.setXCallUUID(xcallid);
                }
                signallingStats.setCallUUID(xcallid);
                callInsights.sendSummaryEvent(signallingStats);
                callInsights.stopTimer();
            }

            if(statusCode >= 404 && statusCode <= 408){
                // invalid call
                cleanupCallData();
            }

            if(statusCode == 503){
                // invalid call
                cleanupCallData();
            }

            if(statusCode == 200){
                signallingStats.setHangupTime();
                signallingStats.setCallUUID(xcallid);
                callInsights.sendSummaryEvent(signallingStats);
                callInsights.stopTimer();
                cleanupCallData();
            }
        }
    }

    @SuppressLint("NewApi")
    public void incomingCallStates(CallState callState, int statusCode, TerminatedReason terminateReason){
        if(callState != null){
            if(statusCode >= 180 && statusCode <= 183){
                signallingStats = new SignallingStats();
                signallingStats.setPostDialDelay();
                signallingStats.setCallProgressTime();
            }

            if(callState == CallState.CallAccepted){
                signallingStats.setAnswerTime();
                signallingStats.setCallConfirmedTime();
                signallingStats.setXCallUUID(xcallid);
                callInsights.sendAnswerEvent(signallingStats, true);
                callInsights.initRTPStats();
                callInsights.sendRtpStats(signallingStats, pc);
            }
        }

        if(terminateReason != null){
            if(statusCode == 486 || statusCode == 487){
                signallingStats.setHangupTime();
                signallingStats.setXCallUUID(xcallid);
                signallingStats.setCallUUID(xcallid);
                callInsights.sendSummaryEvent(signallingStats);
                callInsights.stopTimer();
                signallingStats.setXCallUUID("");
            }

            if(statusCode == 408){
                // call invalid
                cleanupCallData();
            }

            if(statusCode == 200){
                signallingStats.setHangupTime();
                signallingStats.setXCallUUID(xcallid);
                signallingStats.setCallUUID(xcallid);
                callInsights.sendSummaryEvent(signallingStats);
                callInsights.stopTimer();
                signallingStats.setXCallUUID("");
                cleanupCallData();
            }

            if(statusCode == 603){
                // incoming call rejected
                cleanupCallData();
            }
        }
    }

    private void cleanupCallData(){
        signallingStats = null;
        xcallid = null;
    }

    public void createLocalPeerConnection() {
        final ArrayList<PeerConnection.IceServer> iceServers = new ArrayList<>();
        PeerConnection.RTCConfiguration rtcConfig = new PeerConnection.RTCConfiguration(iceServers);

        rtcConfig.continualGatheringPolicy = GATHER_ONCE;
        rtcConfig.bundlePolicy = PeerConnection.BundlePolicy.MAXCOMPAT;
        rtcConfig.rtcpMuxPolicy = PeerConnection.RtcpMuxPolicy.REQUIRE;
        rtcConfig.tcpCandidatePolicy = PeerConnection.TcpCandidatePolicy.ENABLED;
        PeerConnection.IceServer iceServer = new PeerConnection.IceServer("stun:stun.l.google.com:19302");
        iceServers.add(iceServer);

        pc = peerConnectionFactory.createPeerConnection(rtcConfig, new CustomPeerConnectionObserver("localPeerCreation") {
            @Override
            public void onIceCandidate(IceCandidate iceCandidate) {
                super.onIceCandidate(iceCandidate);
                System.out.printf("incoming icecandidate check %s", iceCandidate.sdp);
                try {
                    JSONObject json = new JSONObject();
                    json.put("type", "candidate");
                    json.put("label", iceCandidate.sdpMLineIndex);
                    json.put("id", iceCandidate.sdpMid);
                    json.put("candidate", iceCandidate.sdp);
                    iceCandidate(iceCandidate.sdpMid, iceCandidate.sdp);
//                    sipController.iceCandidate(iceCandidate.sdpMid, iceCandidate.sdp);
                    System.out.println(json.toString());
                    if(!didSentInvite){
                        iceGatheringFinish();
                    }
                    didSentInvite = true;
                } catch (JSONException e) {
                    e.printStackTrace();
                }
            }


            @Override
            public void onAddStream(MediaStream mediaStream) {
                System.out.println("stream added");
                super.onAddStream(mediaStream);
                audioManager.setMode(AudioManager.MODE_IN_COMMUNICATION);
                audioManager.setSpeakerphoneOn(false);
                //AudioTrack temp = peerConnection.

            }

            @Override
            public void onIceConnectionChange(PeerConnection.IceConnectionState iceConnectionState) {
                super.onIceConnectionChange(iceConnectionState);
                System.out.printf("iceconnectionstate: %s \n", iceConnectionState);
            }

            @Override
            public void onIceGatheringChange(PeerConnection.IceGatheringState iceGatheringState) {
                super.onIceGatheringChange(iceGatheringState);
                if (iceGatheringState == PeerConnection.IceGatheringState.COMPLETE){
                    isIceGatheringCompleted = true;
//                    createLocalPeerConnection();

                    //MediaStream stream = peerConnectionFactory.createLocalMediaStream("102");
                    //stream.addTrack(localAudioTrack);
                    //peerConnection.addStream(stream);
                    System.out.println(pc.signalingState());
//                    sipController.iceGatheringFinish();

                }

            }
        });
        addStreamToLocalPeer();
    }

    public void createLocalOffer(String dest, Map<String, String> headers ) {
        pc.createOffer(new CustomSdpObserver("localCreateOffer") {
            @Override
            public void onCreateSuccess(SessionDescription sessionDescription) {
                super.onCreateSuccess(sessionDescription);
                System.out.println(pc.signalingState());
                pc.setLocalDescription(new CustomSdpObserver("localSetLocalDesc"), sessionDescription);
                try {
                    JSONObject json = new JSONObject();
                    json.put("type", sessionDescription.type);
                    json.put("sdp", sessionDescription.description);
                    System.out.println(json.toString());
                    try {
                        System.out.println("Signalling state before call");
                        System.out.println(pc.signalingState());
                        temp = pc;
                        System.out.println(pc);
                        HashMap<String, String> headers = new HashMap<String, String>();
                        headers.put("","");
                        makeCall(dest,sessionDescription.description, headers.toString());
                    }catch (Exception e){
                        e.printStackTrace();
                    }
                    //}
                    System.out.println(json.toString());
                } catch (JSONException e) {
                    e.printStackTrace();
                }
            }


        }, sdpConstraints);
    }

    public void addStreamToLocalPeer() {
        //stream = peerConnectionFactory.createLocalMediaStream("101");
        //stream.addTrack(localAudioTrack);
        pc.addStream(stream);
        System.out.println("printing stream");
        //System.out.println(peer);
    }

    public void setActivityContext(Context ctx) {
        context = ctx;
    }

    public void setAudioManager(AudioManager audio) {
        audioManager = audio;
    }

    public void initializePeerConnection() {
        //Initialize PeerConnectionFactory globals.
        //Params are context, initAudio,initVideo and videoCodecHwAcceleration
        // create PeerConnectionFactor
        System.out.println("initializing Peer Connection");

        PeerConnectionFactory.InitializationOptions initializationOptions =
                PeerConnectionFactory.InitializationOptions.builder(context).createInitializationOptions();
        PeerConnectionFactory.initialize(initializationOptions);

        PeerConnectionFactory.Options options = new PeerConnectionFactory.Options();
        peerConnectionFactory = PeerConnectionFactory.builder().setOptions(options).createPeerConnectionFactory();

        //initialise media constraints
        MediaConstraints audioConstraints = new MediaConstraints();
        boolean enableAudio = true;


        //create media stream
        stream = peerConnectionFactory.createLocalMediaStream("101");

        //create an AudioSource instance
        AudioSource audioSource = peerConnectionFactory.createAudioSource(audioConstraints);
        localAudioTrack = peerConnectionFactory.createAudioTrack("101", audioSource);
        localAudioTrack.setEnabled(enableAudio);
        stream.addTrack(localAudioTrack);



        //localAudioTrack = createAudioTrack();
        //Create MediaConstraints - Will be useful for specifying video and audio constraints.
        //MediaConstraints audioConstraints = new MediaConstraints();
        sdpConstraints = new MediaConstraints();

        sdpConstraints.mandatory.add(new MediaConstraints.KeyValuePair("offerToReceiveAudio", "true"));
        sdpConstraints.mandatory.add(new MediaConstraints.KeyValuePair("offerToReceiveVideo", "false"));

    }

    public void doAnswer() {
        System.out.printf("doanswer: %s \n", pc.signalingState());
        pc.createAnswer(new CustomSdpObserver("localCreateAnswer") {
            @Override
            public void onCreateSuccess(SessionDescription sessionDescription) {
                pc.setLocalDescription(new CustomSdpObserver("localSetDesc"), sessionDescription);

                JSONObject message = new JSONObject();
                try {
                    message.put("type", "answer");
                    message.put("sdp", sessionDescription.description);
                    System.out.printf("set local sdp\n%s",sessionDescription.description);
                    answer(sessionDescription.description);
                } catch (JSONException e) {
                    e.printStackTrace();
                }
            }
        }, new MediaConstraints());
    }


    public void hangup() {
        pc.close();
        pc = null;
        didSentInvite = false;
    }

    public void sendFeedbackEvent(JSONObject feedbackEvent){
        callInsights.sendFeedbackEvent(feedbackEvent);
    }

    public void login(String username, String password, String token){
        this.username = username;
        this.password = password;
        registerUserToken(username, password, token);
        initializePeerConnection();
    }


    public synchronized void onRegistration(RegistrationEvent event, String user) {
        Log.d(TAG, "onRegistration");
        if (onRegistrationEventListener != null){
            if(event == RegistrationEvent.REGISTERED){
                callInsights = new CallInsights(this.username, this.password, "phone.plivo.com", new HashMap<String, Boolean>());
            }
            onRegistrationEventListener.onRegistrationEvent(event, user);
        }
    }

    public synchronized void onCall(CallEvent event, String user) {
        Log.d(TAG, "onCall");
        if (onCallEventListener != null){
            if(event == CallEvent.INCOMING_CALL){
                isIncomingCall = true;
            }
            onCallEventListener.onCallEvent(event, user);
        }
    }

    public synchronized void onLog(String log) {
        Log.d(TAG, "onLog: " + log);
        if (onLogEventListener != null)
            onLogEventListener.onLogEvent(log);
    }

    public synchronized void onError(ErrorType type, String error) {
        Log.d(TAG, "onError: " + error);
        if (onErrorEventListener != null)
            onErrorEventListener.onErrorEvent(type, error);
    }

    public synchronized void registerOnRegistrationEventListener(OnRegistrationEventListener listener) {
        onRegistrationEventListener = listener;
    }

    public synchronized void registerOnCallEventListener(OnCallEventListener listener) {
        onCallEventListener = listener;
    }

    public synchronized void registerOnLogEventListener(OnLogEventListener listener) {
        onLogEventListener = listener;
    }

    public synchronized void registerOnErrorEventListener(OnErrorEventListener listener) {
        onErrorEventListener = listener;
    }

    public native String test(String testName);
    public native void answer(String sdp);
    public native void init(Context context, ServerSettings serverSettings);
    public native void setHasAudio(boolean hasAudio);
    public native void setHasVideo(boolean hasVideo);
    public native void registerUser(String username, String password);
    public native void registerUserToken(String username, String password, String token);
    public native void registerUserTokenHeaders(String username, String password, String token, String proxy, Map<String, String>headers, String label, String index);
    public native void unregisterUser();
    public native void makeCall(String sipUri, String localSDP, String headers);
    //public native void answer();
    public native void endCall(boolean destroyLocalStream);
    public native void setLocalView(long renderer);
    public native void setRemoteView(long renderer);
    public native void iceGatheringFinish();
    public native void iceCandidate(String mid, String sdp);
}


