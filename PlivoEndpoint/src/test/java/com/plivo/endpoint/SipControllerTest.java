package com.plivo.endpoint;

import static org.junit.Assert.*;
import org.junit.Before;
import org.junit.Test;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.webrtc.PeerConnection;

public class SipControllerTest {
    SipController sipController;

    @Before
    public void setUp() throws Exception {
//        sipController = SipController.getInstance();
    }

    @Test
    public void getXCallUUID() {
        assertEquals("123",sipController.getXCallUUID("X-CallUUID:123"));
    }

    @Test
    public void getInstance() {
//        assertNotNull(SipController.getInstance());
    }

//    @Test
//    public void setRemoteSDP() {
//        sipController.initializePeerConnection();
//        sipController.createLocalPeerConnection();
//        System.out.println(sipController.pc);
////        PeerConnection pc = Mockito.mock(sipController.pc.getClass());
//////        sipController.setRemoteSDP();
////        Mockito.verify(pc, Mockito.times(0));
//    }

    @Test
    public void sendDigits() {
    }

    @Test
    public void setCallInfo() {
        sipController.setCallInfo("X-CallUUID:123");
        assertEquals("123", sipController.getXCallUUID("X-CallUUID:123"));
    }

    @Test
    public void setCallState() {
    }

    @Test
    public void outgoingCallStates() {
    }

    @Test
    public void incomingCallStates() {
    }

    @Test
    public void createLocalPeerConnection() {
    }

    @Test
    public void createLocalOffer() {
    }

    @Test
    public void addStreamToLocalPeer() {
    }

    @Test
    public void setActivityContext() {
    }

    @Test
    public void setAudioManager() {
    }

    @Test
    public void initializePeerConnection() {
    }

    @Test
    public void doAnswer() {
    }

    @Test
    public void hangup() {
    }

    @Test
    public void sendFeedbackEvent() {
    }

    @Test
    public void onRegistration() {
    }

    @Test
    public void onCall() {
    }

    @Test
    public void onLog() {
    }

    @Test
    public void onError() {
    }

    @Test
    public void registerOnRegistrationEventListener() {
    }

    @Test
    public void registerOnCallEventListener() {
    }

    @Test
    public void registerOnLogEventListener() {
    }

    @Test
    public void registerOnErrorEventListener() {
    }

    @Test
    public void test1() {
    }

    @Test
    public void answer() {
    }

    @Test
    public void init() {
    }

    @Test
    public void setHasAudio() {
    }

    @Test
    public void setHasVideo() {
    }

    @Test
    public void registerUser() {
    }

    @Test
    public void registerUserToken() {
    }

    @Test
    public void registerUserTokenHeaders() {
    }

    @Test
    public void unregisterUser() {
    }

    @Test
    public void makeCall() {
    }

    @Test
    public void endCall() {
    }

    @Test
    public void setLocalView() {
    }

    @Test
    public void setRemoteView() {
    }

    @Test
    public void iceGatheringFinish() {
    }

    @Test
    public void iceCandidate() {
    }
}