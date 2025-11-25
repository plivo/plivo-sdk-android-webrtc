#include <android/log.h>

#include "SipControllerCore.h"
#include "/Users/karthikdechiraju/Library/Android/ndk/android-ndk-r12b/platforms/android-21/arch-x86/usr/include/jni.h"


using namespace rtcsip;
//using namespace webrtc_jni;

class SipControllerHandler;

static JavaVM *g_jvm = NULL;
static jobject g_sipControllerJava;
static jclass g_registrationEventEnum;
static jclass g_callEventEnum;
static jclass g_errorTypeEnum;
static SipControllerCore *g_sipControllerCore = NULL;

static SipControllerHandler *g_sipControllerHandler = NULL;
static SipSDPHandler *g_sdpHandler = NULL;


extern "C" {

JNIEXPORT jstring JNICALL Java_com_plivo_endpoint_SipController_test
        (JNIEnv *env, jobject obj, jstring testname);

JNIEXPORT void JNICALL Java_com_plivo_endpoint_SipController_init
    (JNIEnv *env, jobject obj, jobject context, jobject j_settings);

JNIEXPORT void JNICALL Java_com_plivo_endpoint_SipController_setHasAudio
    (JNIEnv *, jobject, jboolean hasAudio);

JNIEXPORT void JNICALL Java_com_plivo_endpoint_SipController_setHasVideo
    (JNIEnv *, jobject, jboolean hasVideo);

JNIEXPORT void JNICALL Java_com_plivo_endpoint_SipController_registerUser
    (JNIEnv *env, jobject, jstring j_username, jstring j_password);

JNIEXPORT void JNICALL Java_com_plivo_endpoint_SipController_registerUserToken
        (JNIEnv *env, jobject, jstring j_username, jstring j_password, jstring j_token);

JNIEXPORT void JNICALL Java_com_plivo_endpoint_SipController_registerUserTokenHeaders
        (JNIEnv *env, jobject, jstring j_username, jstring j_password, jstring j_token, jstring j_proxy,
         jobject j_headers, jstring label, jstring index);

JNIEXPORT void JNICALL Java_com_plivo_endpoint_SipController_iceGatheringFinish
        (JNIEnv *env, jobject);

JNIEXPORT void JNICALL Java_com_plivo_endpoint_SipController_iceCandidate
        (JNIEnv *env, jobject, jstring j_mid, jstring j_sdp);

JNIEXPORT void JNICALL Java_com_plivo_endpoint_SipController_unregisterUser
    (JNIEnv *, jobject);

JNIEXPORT void JNICALL Java_com_plivo_endpoint_SipController_makeCall
    (JNIEnv *env, jobject, jstring j_sipUri, jstring j_localSDP, jstring j_headers);

JNIEXPORT void JNICALL Java_com_plivo_endpoint_SipController_answer
    (JNIEnv *, jobject, jstring sdp);

JNIEXPORT void JNICALL Java_com_plivo_endpoint_SipController_endCall
    (JNIEnv *env, jobject, jboolean destroyLocalStream);

JNIEXPORT void JNICALL Java_com_plivo_endpoint_SipController_setVideoCapturer
    (JNIEnv *, jobject, jlong j_capturer_pointer, jobject j_capture_constraints);

JNIEXPORT void JNICALL Java_com_plivo_endpoint_SipController_setLocalView
    (JNIEnv *, jobject, jlong j_renderer_pointer);

JNIEXPORT void JNICALL Java_com_plivo_endpoint_SipController_setRemoteView
    (JNIEnv *, jobject, jlong j_renderer_pointer);



class SipControllerHandler : public SipRegistrationHandler, public SipCallHandler, public SipLogHandler,
                             public SipSDPHandler, public SipErrorHandler, public SipCallInfoHandler, public SipCallStateHandler
{
public:

    virtual void handleSDP(std::string sdp){
        JNIEnv *env;
        bool isAttached = false;

        if (g_jvm->GetEnv((void**) &env, JNI_VERSION_1_6) != JNI_OK)
        {
            g_jvm->AttachCurrentThread(&env, NULL);
            isAttached = true;
        }
        jclass j_sipControllerClass = env->GetObjectClass(g_sipControllerJava);

        jmethodID j_midOnCall = env->GetMethodID(j_sipControllerClass, "setRemoteSDP",
                                                 "(Ljava/lang/String;)V");

        jfieldID j_fidCallEvent;

        jstring j_sdp = env->NewStringUTF(sdp.c_str());
        env->CallVoidMethod(g_sipControllerJava, j_midOnCall,j_sdp);

        if (isAttached)
            g_jvm->DetachCurrentThread();

    }
    virtual void handleRegistration(SipRegistrationEvent event, std::string user)
    {
        JNIEnv *env;
        bool isAttached = false;

        if (g_jvm->GetEnv((void**) &env, JNI_VERSION_1_6) != JNI_OK)
        {
            g_jvm->AttachCurrentThread(&env, NULL);
            isAttached = true;
        }

        jclass j_sipControllerClass = env->GetObjectClass(g_sipControllerJava);

        jmethodID j_midOnRegistration = env->GetMethodID(j_sipControllerClass, "onRegistration",
                                                         "(Lcom/plivo/endpoint/SipController$RegistrationEvent;Ljava/lang/String;)V");
        jfieldID j_fidRegistrationEvent;
        if (event == SipRegistrationEvent::Registered)
            j_fidRegistrationEvent = env->GetStaticFieldID(g_registrationEventEnum, "REGISTERED",
                                                           "Lcom/plivo/endpoint/SipController$RegistrationEvent;");
        else if (event == SipRegistrationEvent::NotRegistered)
            j_fidRegistrationEvent = env->GetStaticFieldID(g_registrationEventEnum, "NOT_REGISTERED",
                                                           "Lcom/plivo/endpoint/SipController$RegistrationEvent;");
        else
            return;

        jobject j_registrationEvent = env->GetStaticObjectField(g_registrationEventEnum, j_fidRegistrationEvent);

        jstring j_user = env->NewStringUTF(user.c_str());

        env->CallVoidMethod(g_sipControllerJava, j_midOnRegistration, j_registrationEvent, j_user);

        if (isAttached)
            g_jvm->DetachCurrentThread();
    }

    virtual void handleCall(SipCallEvent event, std::string user)
    {
        JNIEnv *env;
        bool isAttached = false;

        if (g_jvm->GetEnv((void**) &env, JNI_VERSION_1_6) != JNI_OK)
        {
            g_jvm->AttachCurrentThread(&env, NULL);
            isAttached = true;
        }

        jclass j_sipControllerClass = env->GetObjectClass(g_sipControllerJava);

        jmethodID j_midOnCall = env->GetMethodID(j_sipControllerClass, "onCall",
                                                 "(Lcom/plivo/endpoint/SipController$CallEvent;Ljava/lang/String;)V");

        jfieldID j_fidCallEvent;
        if (event == SipCallEvent::IncomingCall)
            j_fidCallEvent = env->GetStaticFieldID(g_callEventEnum, "INCOMING_CALL", "Lcom/plivo/endpoint/SipController$CallEvent;");
        else if (event == SipCallEvent::TerminateCall)
            j_fidCallEvent = env->GetStaticFieldID(g_callEventEnum, "TERMINATE_CALL", "Lcom/plivo/endpoint/SipController$CallEvent;");
        else if (event == SipCallEvent::CallAccepted)
            j_fidCallEvent = env->GetStaticFieldID(g_callEventEnum, "CALL_ACCEPTED", "Lcom/plivo/endpoint/SipController$CallEvent;");
        else
            return;

        jobject j_callEvent = env->GetStaticObjectField(g_callEventEnum, j_fidCallEvent);

        jstring j_user = env->NewStringUTF(user.c_str());

        env->CallVoidMethod(g_sipControllerJava, j_midOnCall, j_callEvent, j_user);

        if (isAttached)
            g_jvm->DetachCurrentThread();
    }

    virtual void handleLog(std::string log)
    {
        JNIEnv *env;
        bool isAttached = false;

        if (g_jvm->GetEnv((void**) &env, JNI_VERSION_1_6) != JNI_OK)
        {
            g_jvm->AttachCurrentThread(&env, NULL);
            isAttached = true;
        }

        jclass j_sipControllerClass = env->GetObjectClass(g_sipControllerJava);

        jmethodID j_midOnLog = env->GetMethodID(j_sipControllerClass, "onLog", "(Ljava/lang/String;)V");

        jstring j_log = env->NewStringUTF(log.c_str());

        env->CallVoidMethod(g_sipControllerJava, j_midOnLog, j_log);

        if (isAttached)
            g_jvm->DetachCurrentThread();
    }

    virtual void handleError(SipErrorType type, std::string error)
    {
        JNIEnv *env;
        bool isAttached = false;

        if (g_jvm->GetEnv((void**) &env, JNI_VERSION_1_6) != JNI_OK)
        {
            g_jvm->AttachCurrentThread(&env, NULL);
            isAttached = true;
        }

        jclass j_sipControllerClass = env->GetObjectClass(g_sipControllerJava);

        jmethodID j_midOnError = env->GetMethodID(j_sipControllerClass, "onError",
                                                  "(Lcom/plivo/endpoint/SipController$ErrorType;Ljava/lang/String;)V");
        jfieldID j_fidErrorType;
        if (type == SipErrorType::WebRtcError)
            j_fidErrorType = env->GetStaticFieldID(g_errorTypeEnum, "WEBRTC_ERROR", "Lcom/plivo/endpoint/SipController$ErrorType;");
        else if (type == SipErrorType::SipConnectionError)
            j_fidErrorType = env->GetStaticFieldID(g_errorTypeEnum, "SIP_CONNECTION_ERROR",
                                                   "Lcom/plivo/endpoint/SipController$ErrorType;");
        else if (type == SipErrorType::SipSessionError)
            j_fidErrorType = env->GetStaticFieldID(g_errorTypeEnum, "SIP_SESSION_ERROR",
                                                   "Lcom/plivo/endpoint/SipController$ErrorType;");
        else
            return;

        jobject j_errorType = env->GetStaticObjectField(g_errorTypeEnum, j_fidErrorType);

        jstring j_error = env->NewStringUTF(error.c_str());

        env->CallVoidMethod(g_sipControllerJava, j_midOnError, j_errorType, j_error);

        if (isAttached)
            g_jvm->DetachCurrentThread();
    }

    virtual void handleCallInfo(std::string callinfo){
        JNIEnv *env;
        bool isAttached = false;

        if (g_jvm->GetEnv((void**) &env, JNI_VERSION_1_6) != JNI_OK)
        {
            g_jvm->AttachCurrentThread(&env, NULL);
            isAttached = true;
        }
        jclass j_sipControllerClass = env->GetObjectClass(g_sipControllerJava);

        jmethodID j_midOnCall = env->GetMethodID(j_sipControllerClass, "setCallInfo",
                                                 "(Ljava/lang/String;)V");

        jfieldID j_fidCallEvent;

        jstring j_sdp = env->NewStringUTF(callinfo.c_str());
        env->CallVoidMethod(g_sipControllerJava, j_midOnCall,j_sdp);

        if (isAttached)
            g_jvm->DetachCurrentThread();
    }
    virtual void handleCallState(std::string state, int statusCode){
        JNIEnv *env;
        bool isAttached = false;

        if (g_jvm->GetEnv((void**) &env, JNI_VERSION_1_6) != JNI_OK)
        {
            g_jvm->AttachCurrentThread(&env, NULL);
            isAttached = true;
        }
        jclass j_sipControllerClass = env->GetObjectClass(g_sipControllerJava);

        jmethodID j_midOnCall = env->GetMethodID(j_sipControllerClass, "setCallState",
                                                 "(Ljava/lang/String;I)V");

        jfieldID j_fidCallEvent;

        jstring j_state = env->NewStringUTF(state.c_str());
        env->CallVoidMethod(g_sipControllerJava, j_midOnCall,j_state, statusCode);
        if (isAttached)
            g_jvm->DetachCurrentThread();
    }
};

JNIEXPORT jstring JNICALL Java_com_plivo_endpoint_SipController_test
        (JNIEnv *env, jobject obj, jstring j_settings) {
    std::string hello = "Hello from World";
    return env->NewStringUTF(hello.c_str());
}

JNIEXPORT void JNICALL Java_com_plivo_endpoint_SipController_init
        (JNIEnv *env, jobject obj, jobject context, jobject j_settings)
{
    g_sipControllerJava = env->NewGlobalRef(obj);

    //webrtc::VoiceEngine::SetAndroidObjects(g_jvm, context);
    //webrtc::SetRenderAndroidVM(g_jvm);
    //AndroidVideoCapturerJni::SetAndroidObjects(env, context);

    SipServerSettings serverSettings;

    jclass j_serverSettingsClass = env->GetObjectClass(j_settings);

    jfieldID j_fidDomain = env->GetFieldID(j_serverSettingsClass, "domain", "Ljava/lang/String;");
    jfieldID j_fidDnsServer = env->GetFieldID(j_serverSettingsClass, "dnsServer", "Ljava/lang/String;");
    jfieldID j_fidProxyServer = env->GetFieldID(j_serverSettingsClass, "proxyServer", "Ljava/lang/String;");

    jstring j_domain = static_cast<jstring>(env->GetObjectField(j_settings, j_fidDomain));
    jstring j_dnsServer = static_cast<jstring>(env->GetObjectField(j_settings, j_fidDnsServer));
    jstring j_proxyServer = static_cast<jstring>(env->GetObjectField(j_settings, j_fidProxyServer));

    const char *domain = env->GetStringUTFChars(j_domain, NULL);
    const char *dnsServer = env->GetStringUTFChars(j_dnsServer, NULL);
    const char *proxyServer = env->GetStringUTFChars(j_proxyServer, NULL);

    serverSettings.domain = domain;
    serverSettings.dnsServer = dnsServer;
    serverSettings.proxyServer = proxyServer;

    env->ReleaseStringUTFChars(j_domain, domain);
    env->ReleaseStringUTFChars(j_dnsServer, dnsServer);
    env->ReleaseStringUTFChars(j_proxyServer, proxyServer);

    //g_webRtcEngine = new WebRtcEngine();

    g_sipControllerCore = new SipControllerCore(serverSettings);
    g_sipControllerHandler = new SipControllerHandler();
    //g_sdpHandler =  new SipSDPHandler();
    g_sipControllerCore->registerRegistrationHandler(g_sipControllerHandler);
    g_sipControllerCore->registerCallHandler(g_sipControllerHandler);
    g_sipControllerCore->registerLogHandler(g_sipControllerHandler);
    g_sipControllerCore->registerErrorHandler(g_sipControllerHandler);
    g_sipControllerCore->sipSDPHandler(g_sipControllerHandler);
    g_sipControllerCore->sipCallInfoHandler(g_sipControllerHandler);
    g_sipControllerCore->sipCallStateHandler(g_sipControllerHandler);

}


//Android ---> java class(ainActivity.java) ----> jni interface (rtcsip_jin.cpp) ----> C++ (Sipcontroller.cpp) ---------> resiprocate library

//

JNIEXPORT void JNICALL Java_com_plivo_endpoint_SipController_setHasAudio
        (JNIEnv *, jobject, jboolean hasAudio)
{
    //g_webRtcEngine->setHasAudio(hasAudio);
}

JNIEXPORT void JNICALL Java_com_plivo_endpoint_SipController_setHasVideo
        (JNIEnv *, jobject, jboolean hasVideo)
{
    //g_webRtcEngine->setHasVideo(hasVideo);
}

JNIEXPORT void JNICALL Java_com_plivo_endpoint_SipController_iceGatheringFinish
        (JNIEnv *env, jobject)
{
    g_sipControllerCore->onIceGatheringFinished();
}

JNIEXPORT void JNICALL Java_com_plivo_endpoint_SipController_iceCandidate
        (JNIEnv *env, jobject, jstring j_mid, jstring j_sdp)
{
    std::string mid =env->GetStringUTFChars(j_mid, 0);
    std::string sdp =env->GetStringUTFChars(j_sdp, 0);
    g_sipControllerCore->onIceCandidate(sdp, mid);
}

JNIEXPORT void JNICALL Java_com_plivo_endpoint_SipController_registerUser
        (JNIEnv *env, jobject, jstring j_username, jstring j_password)
{
    const char *username = env->GetStringUTFChars(j_username, NULL);
    const char *password = env->GetStringUTFChars(j_password, NULL);
    g_sipControllerCore->registerUser(username, password);

    env->ReleaseStringUTFChars(j_username, username);
    env->ReleaseStringUTFChars(j_password, password);
}

JNIEXPORT void JNICALL Java_com_plivo_endpoint_SipController_registerUserToken
        (JNIEnv *env, jobject, jstring j_username, jstring j_password, jstring j_token)
{
    const char *username = env->GetStringUTFChars(j_username, NULL);
    const char *password = env->GetStringUTFChars(j_password, NULL);
    const char *token = env->GetStringUTFChars(j_token, NULL);
    g_sipControllerCore->registerUser(username, password, token);

    env->ReleaseStringUTFChars(j_username, username);
    env->ReleaseStringUTFChars(j_password, password);
}

JNIEXPORT void JNICALL Java_com_plivo_endpoint_SipController_registerUserTokenHeaders
        (JNIEnv *env, jobject, jstring j_username, jstring j_password, jstring j_token, jstring j_proxy,
         jobject j_headers, jstring j_label, jstring j_index)
{
    const char *username = env->GetStringUTFChars(j_username, NULL);
    const char *password = env->GetStringUTFChars(j_password, NULL);
    const char *label = env->GetStringUTFChars(j_label, NULL);
    const char *index = env->GetStringUTFChars(j_index, NULL);
    const char *token = env->GetStringUTFChars(j_token, NULL);
    const char *proxy = env->GetStringUTFChars(j_proxy, NULL);



    map<string, string> headers;
    headers["X-PH-conference"] = "true";
    headers["X-Label"] = label;
    headers["X-Index"] = index;
    g_sipControllerCore->registerUser(username, password, token, "NA", proxy, headers);

    env->ReleaseStringUTFChars(j_username, username);
    env->ReleaseStringUTFChars(j_password, password);
}

JNIEXPORT void JNICALL Java_com_plivo_endpoint_SipController_unregisterUser
        (JNIEnv *, jobject)
{
    g_sipControllerCore->unregisterUser();
}

JNIEXPORT void JNICALL Java_com_plivo_endpoint_SipController_makeCall
        (JNIEnv *env, jobject, jstring j_sipUri, jstring j_localSdp, jstring j_headers)
{

    const char *sipUri = env->GetStringUTFChars(j_sipUri, NULL);
    const char *localSDP = env->GetStringUTFChars(j_localSdp, NULL);
    g_sipControllerCore->createSession(sipUri, localSDP);

    env->ReleaseStringUTFChars(j_sipUri, sipUri);
}

JNIEXPORT void JNICALL Java_com_plivo_endpoint_SipController_answer
        (JNIEnv *env, jobject,jstring j_localSdp)
{
    //g_webRtcEngine->setVideoCapturer(g_capturer, g_captureConstraints);
    //g_webRtcEngine->setLocalRenderer(g_localRenderer);
    //g_webRtcEngine->setRemoteRenderer(g_remoteRenderer);
    const char *localSDP = env->GetStringUTFChars(j_localSdp, NULL);

    g_sipControllerCore->acceptSession(localSDP);
}

JNIEXPORT void JNICALL Java_com_plivo_endpoint_SipController_endCall
        (JNIEnv *env, jobject, jboolean destroyLocalStream)
{
    g_sipControllerCore->terminateSession(destroyLocalStream);
}

/*JNIEXPORT void JNICALL Java_com_plivo_endpoint_SipController_setVideoCapturer
        (JNIEnv *env, jobject, jlong j_capturer_pointer, jobject j_capture_constraints)
{
    rtc::scoped_ptr<ConstraintsWrapper> constraints(
                new ConstraintsWrapper(env, j_capture_constraints));
    g_capturer = reinterpret_cast<cricket::VideoCapturer*>(j_capturer_pointer);
    g_captureConstraints = constraints.release();
}*/

JNIEXPORT void JNICALL Java_com_plivo_endpoint_SipController_setLocalView
        (JNIEnv *, jobject, jlong j_renderer_pointer)
{
    //g_localRenderer = reinterpret_cast<webrtc::VideoRendererInterface*>(j_renderer_pointer);
}

JNIEXPORT void JNICALL Java_com_plivo_endpoint_SipController_setRemoteView
        (JNIEnv *, jobject, jlong j_renderer_pointer)
{
    //g_remoteRenderer = reinterpret_cast<webrtc::VideoRendererInterface*>(j_renderer_pointer);
}


JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *jvm, void *reserved)
{
    __android_log_print(ANDROID_LOG_VERBOSE, "rtcsip_jni", "JNI_OnLoad");

    g_jvm = jvm;

    JNIEnv* env;
    if (jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK)
        return -1;

    jclass j_registrationEventEnum = env->FindClass(
            "com/plivo/endpoint/SipController$RegistrationEvent");
    g_registrationEventEnum = reinterpret_cast<jclass>(env->NewGlobalRef(j_registrationEventEnum));

    jclass j_callEventEnum = env->FindClass("com/plivo/endpoint/SipController$CallEvent");
    g_callEventEnum = reinterpret_cast<jclass>(env->NewGlobalRef(j_callEventEnum));

    jclass j_errorTypeEnum = env->FindClass("com/plivo/endpoint/SipController$ErrorType");
    g_errorTypeEnum = reinterpret_cast<jclass>(env->NewGlobalRef(j_errorTypeEnum));
    return JNI_VERSION_1_4;
//    jint ret = InitGlobalJniVariables(jvm);
//    if (ret < 0)
//      return -1;
//
//    //LoadGlobalClassReferenceHolder();
//
//    return ret;
}

JNIEXPORT void JNICALL JNI_OnUnLoad(JavaVM *jvm, void *reserved)
{
    __android_log_print(ANDROID_LOG_VERBOSE, "rtcsip_jni", "JNI_OnUnLoad");
    //FreeGlobalClassReferenceHolder();
}
}
