package com.plivo.endpoint;

import static org.junit.Assert.*;

import org.json.JSONObject;
import org.junit.Before;
import org.junit.Test;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.webrtc.PeerConnection;
import org.webrtc.PeerConnectionFactory;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.Timer;
public class CallInsightsTest {
    CallInsights callInsights;

    @Before
    public void setUp() throws Exception {
        callInsights = new CallInsights("testusername", "testpassword", "phone.plivo.com", new HashMap<String, Boolean>());
    }

    @Test
    public void initRTPStats() {
        callInsights.initRTPStats();
        assertNotNull(callInsights.getRtpStats());
    }

    @Test
    public void initOptions() {
        callInsights.initOptions(new HashMap<String, Boolean>());
        assertNotNull(callInsights.getOptions());
    }

    @Test
    public void sendAnswerEvent_false() {
        SignallingStats signallingStats = new SignallingStats();
        ArrayList<JSONObject> statsBuffer = callInsights.getStatBuffer();
        callInsights.setStatsKey("1234");
        callInsights.sendAnswerEvent(signallingStats, false);
        assertEquals(1, statsBuffer.size());
    }

    @Test
    public void sendAnswerEvent_true() {
        SignallingStats signallingStats = new SignallingStats();
        ArrayList<JSONObject> statsBuffer = callInsights.getStatBuffer();
        callInsights.setStatsKey("1234");
        callInsights.sendAnswerEvent(signallingStats, true);
        assertEquals(1, statsBuffer.size());
    }

    @Test
    public void sendSummaryEvent() {
        SignallingStats signallingStats = new SignallingStats();
        ArrayList<JSONObject> statsBuffer = callInsights.getStatBuffer();
        callInsights.setStatsKey("1234");
        callInsights.sendSummaryEvent(signallingStats);
        assertEquals(1, statsBuffer.size());
    }

    @Test
    public void sendFeedbackEvent() {
        JSONObject jsonObject = new JSONObject();
        ArrayList<JSONObject> statsBuffer = callInsights.getStatBuffer();
        callInsights.setStatsKey("1234");
        callInsights.sendFeedbackEvent(jsonObject);
        assertEquals(1, statsBuffer.size());
    }

    @Test
    public void stopTimer() {
    }
}