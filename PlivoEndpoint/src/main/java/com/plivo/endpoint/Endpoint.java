package com.plivo.endpoint;

import android.content.Context;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.TimeUnit;
import org.json.JSONException;
import org.json.JSONObject;

public class Endpoint {
    private static final int MIN_REG_TIMEOUT;
    private static final int MAX_REG_TIMEOUT;
    private static Context context;
    private static HashMap setupOptions;
    private EventListener eventListener;
    private boolean isInitialized;
//    private Outgoing curOutgoing;
    private int regTimeout;
    private boolean isRegistered;
    private boolean isLogoutInProgress;
    private boolean isRegWithDeviceToken;
    private String userName;
    private String password;
    SipController sipController;
    private Outgoing curOutgoing;

    public Endpoint(boolean debug, EventListener eventListener) {
        this.regTimeout = (int)TimeUnit.MINUTES.toSeconds(10L);
        Log.enable(debug);
        this.eventListener = eventListener;
        this.isInitialized = this.initLib();
    }

    public static Endpoint newInstance(boolean debug, EventListener listener) {
        Endpoint endpoint = new Endpoint(debug, listener);
        Log.D("newInstance " + debug);
        return endpoint;
    }

    public static Endpoint newInstance(boolean debug, HashMap options, EventListener eventListener) {
        context = (Context) options.get("context");
        options.remove("context");
        setupOptions = options;
        Endpoint endpoint = new Endpoint(debug, eventListener);
        Log.D("newInstance " + debug);
        return endpoint;
    }

    public boolean login(String username, String password) {
        return this.login(username, password, "");
    }

    public boolean login(String username, String password, int regTimeout) {
        if (regTimeout >= MIN_REG_TIMEOUT && regTimeout <= MAX_REG_TIMEOUT) {
            this.regTimeout = regTimeout;
            return this.login(username, password, "");
        } else {
            Log.E("Allowed values of regTimeout are between 120 and 86400 seconds only");
            return false;
        }
    }

    public boolean login(String username, String password, String deviceToken) {
        return this.login(username, password, deviceToken, "");
    }

    public boolean login(String username, String password, String deviceToken, String certificateId) {
        if (!NetworkChangeReceiver.isConnected()) {
            return false;
        } else if (deviceToken == null) {
            Log.E("Device token shouldn't be null. Pass in the token received from FCM.");
            return false;
        } else {
            this.userName = username;
            this.password = password;
            if (this.isRegistered) {
                Log.E("Already logged in with the endpoint. Logout and then login.");
                return true;
            } else {
                this.isRegWithDeviceToken = false;
                if (deviceToken.length() > 0) {
                    this.regTimeout = 2592000;
                    this.isRegWithDeviceToken = true;
                }

//                init call insights

                try {
                    sipController.registerUserToken(username, password, deviceToken);
                    this.logDebug(this.isRegWithDeviceToken ? "Logging in with device token..." : "Logging in...");
                    return true;
                } catch (UnsatisfiedLinkError var6) {
                    var6.printStackTrace();
                    Log.E("errload loading libresipplivo:" + var6.toString());
                } catch (Exception var7) {
                    var7.printStackTrace();
                }

                return false;
            }
        }
    }

    public boolean logout() {
        if (!NetworkChangeReceiver.isConnected()) {
            return false;
        } else if (!this.isRegistered) {
            Log.E("Cannot logout without endpoint already logged in.");
            return false;
        } else if (this.isLogoutInProgress) {
            Log.E("Logout is already in progress. Check for onLogout() callback.");
            return false;
        } else {
            try {
//                if (plivo.Logout() != 0) {
//                    Log.E("Logout failed");
//                    return false;
//                }

                this.logDebug("Logout success");
                this.isLogoutInProgress = true;
                return true;
            } catch (UnsatisfiedLinkError var2) {
                var2.printStackTrace();
                Log.E("errload loading libpjplivo:" + var2.toString());
            } catch (Exception var3) {
                var3.printStackTrace();
            }

            return false;
        }
    }

    public Outgoing createOutgoingCall() {
        this.logDebug("createOutgoingCall");
        if (!NetworkChangeReceiver.isConnected()) {
            return null;
        } else if (!this.isRegistered) {
            Log.E("Cannot createOutgoingCall() without endpoint logged in. Call login() before.");
            return null;
        } else {
            Outgoing out = new Outgoing(this, sipController);
            this.curOutgoing = out;
            Log.I("outgoing object created");
            return out;
        }
    }

    public boolean checkDtmfDigit(String digit) {
        return Utils.VALID_DTMF.contains(digit);
    }

    protected Outgoing getOutgoing() {
        return this.curOutgoing;
    }

    void setRegistered(boolean status) {
        this.isRegistered = status;
        if (!this.isRegistered) {
            this.isLogoutInProgress = false;
            this.isRegWithDeviceToken = false;
        }

    }

    public boolean getRegistered() {
        return this.isRegistered;
    }

//    /** @deprecated */
//    @Deprecated
//    public void setRegTimeout(int regTimeout) {
//        if (regTimeout != this.regTimeout) {
//            Log.W("setRegTimeout will be deprecated in upcoming release. Use login(username, password, regTimeout) instead");
//            if (!this.isRegistered) {
//                Log.E("Cannot setRegTimeout() without endpoint logged in. Call login() before.");
//            } else if (regTimeout >= MIN_REG_TIMEOUT && regTimeout <= MAX_REG_TIMEOUT) {
//                this.regTimeout = regTimeout;
//
//                try {
////                    plivo.setRegTimeout(regTimeout);
//                } catch (Exception var3) {
//                    var3.printStackTrace();
//                }
//
//            } else {
//                Log.E("Allowed values of regTimeout are between 120 and 86400 seconds only");
//            }
//        }
//    }

    private void logDebug(String str) {
        Log.D("[endpoint]" + str);
    }

    private boolean initLib() {
        this.loadJNI();
        this.logDebug("Starting module..");
        sipController = SipController.getInstance(this.eventListener);

        ServerSettings serverSettings = new ServerSettings();
        serverSettings.domain = Global.DOMAIN;
        serverSettings.dnsServer = "";
        serverSettings.proxyServer = Global.DOMAIN;
        sipController.init(context, serverSettings);
        return true;
    }

    private void loadJNI() {
        try {
            System.loadLibrary("rtcsip_jni");
            Global.isJniLoaded = true;
            this.logDebug("librtcsip loaded");
        } catch (UnsatisfiedLinkError var2) {
            var2.printStackTrace();
            Log.E("errload loading librtcsip:" + var2.toString());
        } catch (Exception var3) {
            var3.printStackTrace();
        }

    }

    public void keepAlive() {
        this.logDebug("keepAlive");

    }

    public void resetEndpoint() {
        this.logDebug("resetEndpoint");

    }

    public void relayVoipPushNotification(Map<String, String> push_headers) {
        this.logDebug("relayVoipPushNotification: " + push_headers);
        if (!this.isRegistered && !this.isRegWithDeviceToken) {
            Log.E("Cannot call relayVoipPushNotification() without successful login with device token. Use login(String username, String password, String deviceToken).");
        } else if (!Utils.validatePushHeaders(push_headers)) {
            Log.E("Invalid Notification");
        } else {
            //        TODO: relayVoipPushNotification
//            try {
//                plivo.relayVoipPushNotification(Utils.mapToString(push_headers));
//            } catch (UnsatisfiedLinkError var3) {
//                var3.printStackTrace();
//                Log.E("errload loading libpjplivo:" + var3.toString());
//            } catch (Exception var4) {
//                var4.printStackTrace();
//            }

        }
    }
//
    public String getLastCallUUID() {
//        TODO: get last callid
        return this.sipController.getXcallid();
    }
//
    public String getCallUUID() {
        return this.sipController.getXcallid();
    }

    public Map<String, String> getValidationStatus(String callUUID, Integer starRating, ArrayList<String> issues, String note, Boolean sendConsoleLogs) {
        Map<String, String> status = new HashMap<>();
        status.put("true", "No Error");
        if (sendConsoleLogs == null) {
            Log.E("Flag 'sendConsoleLogs' can't be null");
            status.put("false", "Flag 'sendConsoleLogs' can't be null");
            return status;
        } else if (!this.isRegistered) {
            Log.E("Cannot submit feedback without endpoint logged in");
            status.put("false", "Cannot submit feedback without endpoint logged in.");
            return status;
        } else if (callUUID != null && !callUUID.isEmpty()) {
            if (starRating != null && starRating > 0 && starRating <= 5) {
                ArrayList<String> issue_final = new ArrayList<>();
                ArrayList<String> issuesNotFromPredefinedList = new ArrayList<>();
                if (starRating > 0 && starRating <= 5) {
                    if (note != null && note != "" && note.length() > 280) {
                        Log.E("Note can be maximum 280 characters");
                        status.put("false", "Note can be maximum 280 characters");
                        return status;
                    }

                    if (starRating != 5 && (issues == null || issues.isEmpty())) {
                        Log.E("Atleast one issue is mandatory for feedback");
                        status.put("false", "Atleast one issue is mandatory for feedback");
                        return status;
                    }

                    if (issues != null && !issues.isEmpty()) {

                        for (String issue : issues) {
                            String _issue = issue.toUpperCase();
                            Log.I("Issue : " + _issue);
                            if (Global.DEFAULT_COMMENTS.containsKey(_issue)) {
                                String extractedIssue = (String) Global.DEFAULT_COMMENTS.get(_issue);
                                Log.I("Extracted Issue : " + extractedIssue);
                                issue_final.add(extractedIssue);
                            } else {
                                issuesNotFromPredefinedList.add(issue);
                            }
                        }

                        issues.removeAll(issuesNotFromPredefinedList);
                    }

                    if (issue_final.isEmpty()) {
                        new HashSet();
                        Set<String> validIssues = Global.DEFAULT_COMMENTS.keySet();
                        if (starRating != 5) {
                            Log.E("Issues must be from the predefined list of issues for feedback -" + validIssues);
                            status.put("false", "Issues must be from the predefined list of issues for feedback -" + validIssues);
                            return status;
                        }

                        Log.D("Feedback with full rating without any Issues or matches from predefined list of issues -" + validIssues);
                    }
                }

                return status;
            } else {
                Log.E("Star rating should be between 1 to 5");
                status.put("false", "Star rating should be between 1 to 5");
                return status;
            }
        } else {
            Log.E("Caller UUID is mandatory");
            status.put("false", "Caller UUID is mandatory");
            return status;
        }
    }

    public static JSONObject getRequestPayload(String callUUID, String userName, String password) {
        HashMap<String, String> consoleBody = new HashMap<>();

        try {
            consoleBody.put("username", userName);
            consoleBody.put("password", "\"" + password + "\"");
            consoleBody.put("domain", Global.DOMAIN);
            consoleBody.put("calluuid", callUUID);
            consoleBody.put("source", Global.SOURCE);
            return new JSONObject(consoleBody.toString());
        } catch (JSONException var6) {
            var6.printStackTrace();
            return null;
        }
    }

    public void submitCallQualityFeedback(final String callUUID, final Integer starRating, final ArrayList<String> issues, final String note, final Boolean sendConsoleLogs, final FeedbackCallback callback) {
        Map<String, String> status = this.getValidationStatus(callUUID, starRating, issues, note, sendConsoleLogs);
        if (status.containsKey("false")) {
            if (callback != null) {
//                callback.onValidationFail((String)status.get("false"));
            } else {
                Log.D("Validation error : " + (String)status.get("false"));
            }

        } else {
            JSONObject postBody = getRequestPayload(callUUID, this.userName, this.password);

            try {
                HttpPostAsyncTask postClient = new HttpPostAsyncTask(postBody, "POST", new HTTPRequestCallback() {
                    public void onResponse(String response) {
                        if (response.equals("")) {
                            Log.E(" s3 url is empty");
                        } else {
                            try {
                                JSONObject jsonObject = new JSONObject(response);
                                String s3URL = jsonObject.get("data").toString();
                                Map<String, Object> feedback = new HashMap<>();
                                String putRequestLoad = new String();
                                new ArrayList();
                                feedback.put("overall", starRating);
                                String _note = note;
                                if (_note == null) {
                                    _note = "";
                                }

                                ArrayList<String> finalIssueList = new ArrayList<>();
                                Iterator<String> var9 = issues.iterator();

                                while(var9.hasNext()) {
                                    String issue = var9.next();
                                    if (!finalIssueList.contains(issue.toLowerCase())) {
                                        finalIssueList.add(issue.toLowerCase());
                                    }
                                }

                                feedback.put("comment", finalIssueList + " " + _note);
                                putRequestLoad = putRequestLoad + feedback.toString() + "\n";
//                                Endpoint.this.backendListener.sendFeedbackStats(finalIssueList, callUUID, starRating, note);
                                if (sendConsoleLogs) {
                                    putRequestLoad = putRequestLoad + Log.deviceLog.toString();
                                }

                                try {
                                    HttpPutAsyncTask putClient = new HttpPutAsyncTask(putRequestLoad, "PUT", new HTTPRequestCallback() {
                                        public void onResponse(String response) {
                                            if (callback != null) {
                                                callback.onSuccess(response);
                                            } else {
                                                Log.D("Success : " + response);
                                            }

                                        }

                                        public void onFailure(int statusCode) {
                                            Log.E("Log file was not uploaded to server");
                                            if (callback != null) {
                                                callback.onFailure(statusCode);
                                            } else {
                                                Log.D("Failure : " + statusCode);
                                            }

                                        }
                                    });
                                    putClient.execute(s3URL);
                                } catch (Exception var11) {
                                    var11.printStackTrace();
                                }
                            } catch (Exception var12) {
                                var12.printStackTrace();
                            }

                        }
                    }

                    public void onFailure(int statusCode) {
                        Log.E(" Error while making the POST request to get s3url");
                        if (callback != null) {
                            callback.onFailure(statusCode);
                        } else {
                            Log.D("Failure : " + Integer.toString(statusCode));
                        }

                    }
                });
                postClient.execute(new String[]{Global.S3BUCKET_API_URL});
            } catch (Exception var10) {
                var10.printStackTrace();
            }

        }
    }

    static {
        MIN_REG_TIMEOUT = (int)TimeUnit.MINUTES.toSeconds(2L);
        MAX_REG_TIMEOUT = (int)TimeUnit.HOURS.toSeconds(24L);
        setupOptions = new HashMap();
    }
}
