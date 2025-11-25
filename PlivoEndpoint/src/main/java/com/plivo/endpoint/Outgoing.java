package com.plivo.endpoint;

import android.text.TextUtils;


import java.util.*;

public class Outgoing extends IO {
    private final Endpoint endpoint;
    SipController sipController;

    public Outgoing(Endpoint endpoint, SipController sipController) {
        this.endpoint = endpoint;
        this.sipController = sipController;
        isOnMute = false;
    }

    /**
     * Call an endpoint.
     * @param dest
     * @return
     */
    public boolean call(String dest) {
        String sipUri;
        Log.D("call " + dest);
        if (!NetworkChangeReceiver.isConnected()) {
            return false;
        }
        if (TextUtils.isEmpty(dest)) {
            Log.E("Call Cannot be Placed. Entered SIP endpoint is empty.");
            return false;
        }
        if (!endpoint.getRegistered()) {
            Log.E("Cannot make call() without endpoint already logged in.");
            return false;
        }

        if(!dest.startsWith("sip:")){
            sipUri = "sip:" + dest + "@" + Global.DOMAIN;
        } else {
            sipUri = dest + "@" + Global.DOMAIN;
        }
        setToContact(sipUri);

        try {
            makeCall(dest);
            Log.D("Call Placed");
            setActive(true);
            return true;
        } catch (UnsatisfiedLinkError ule) {
            ule.printStackTrace();
            Log.E("errload loading libpjplivo:" + ule.toString());
        } catch (Exception e) {
            e.printStackTrace();
            Log.E("call failed");
        }

        setActive(false);
        return false;
    }

    /* Call method with headers */
    /* the map during initialization should be ConcurrentHashMap */
    public boolean callH(String dest, Map<String, String> headers) {
        String sipUri;
        Log.D("callH " + dest + "headers:" + headers);
        if (!NetworkChangeReceiver.isConnected()) {
            return false;
        }
        if (TextUtils.isEmpty(dest)) {
            Log.E("Call Cannot be Placed. Entered SIP endpoint is empty.");
            return false;
        }
        if (!endpoint.getRegistered()) {
            Log.E("Cannot make callH() without endpoint already logged in.");
            return false;
        }

        if(!dest.startsWith("sip:")){
            sipUri = "sip:" + dest + "@" + Global.DOMAIN;
        } else {
            sipUri = dest + "@" + Global.DOMAIN;
        }
        setToContact(sipUri);

        if (!Utils.validateCallHeaders(headers)) {
            Log.W("No Valid Header. Placing call without headers..");
        }

        String headers_str = Utils.mapToString(headers);
        try {
            if(TextUtils.isEmpty(headers_str)){
                makeCall(dest);
            } else {
                makeCallH(dest, headers);
            }
            Log.D("Call Placed");
            setActive(true);
            return true;
        } catch (UnsatisfiedLinkError ule) {
            ule.printStackTrace();
            Log.E("errload loading libpjplivo:" + ule.toString());
        } catch (Exception e) {
            e.printStackTrace();
            Log.E("callH failed");
        }

        setActive(false);
        return false;
    }

    // retaining this to not break the backward compatibility
    public static void checkSpecialCharacters(Map<String, String> map) {
        Utils.checkSpecialCharacters(map);
    }

    public String getCallId() {
        return callId;
    }

    public void setCallId(String callId) {
        this.callId = callId;
    }

    public String toString() {
        String str = "[Plivo Outgoing Call]callId = " + this.callId + ". to = " + this.toContact;
        return str;
    }

    private void makeCall(String dest){
        //Create Local Peer connection
        sipController.createLocalPeerConnection();
        HashMap<String, String> headers = new HashMap<String, String>();
        headers.put("","");
        //Create Local Offer
        sipController.createLocalOffer(dest, headers);
    }

    private void makeCallH(String dest, Map<String, String> headers){
        //Create Local Peer connection
        sipController.createLocalPeerConnection();
        //Create Local Offer
        sipController.createLocalOffer(dest, headers);
    }


}
