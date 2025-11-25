
#include "SipControllerCore.h"

#ifdef ANDROID
#include <android/log.h>
#include <rutil/DnsUtil.hxx>
#include <resip/stack/ExtensionHeader.hxx>

#endif

namespace rtcsip {


    SipControllerCore::SipControllerCore(SipServerSettings serverSettings) :
            m_run(false),
            m_receiveClosed(false),
            m_inCall(false),
            m_isCaller(false),
            m_localSdpSet(false),
            m_remoteSdpSet(false),
            m_localCandidatesCollected(false),
            m_receiver(NULL),
            m_registrationHandler(NULL),
            m_callHandler(NULL),
            m_logHandler(NULL),
            m_sdpHandler(NULL),
            m_callStateHandler(NULL),
            m_errorHandler(NULL)
    {
        m_domain = serverSettings.domain;
        m_dnsServer = serverSettings.dnsServer;
        m_proxyServer = serverSettings.proxyServer;
    }

    SipControllerCore::~SipControllerCore()
    {
        if (m_receiver)
            m_receiver->detach();
        handleLog("SipControllerCore: deinit : ");
    }

    void SipControllerCore::receive(){
        bool run = true;
        while (run){
            if (m_userAgent->hasEvents()){
                m_userAgent->process();
                m_receiveMutex.lock();
                run = m_run;
                sleepMs(100);
                m_receiveMutex.unlock();
            }
        }
        m_receiveMutex.lock();
        m_receiveClosed = true;
        m_receiveMutex.unlock();
        m_receiveCondition.notify_one();
    }



/*
 *
 MARK: Handler setup
 *
 */
    void SipControllerCore::registerRegistrationHandler(SipRegistrationHandler* handler){
        std::lock_guard<std::mutex> lock(m_controllerMutex);
        m_registrationHandler = handler;
    }

    void SipControllerCore::registerCallHandler(SipCallHandler* handler){
        std::lock_guard<std::mutex> lock(m_controllerMutex);
        m_callHandler = handler;
    }

    void SipControllerCore::registerLogHandler(SipLogHandler* handler){
        std::lock_guard<std::mutex> lock(m_controllerMutex);
        m_logHandler = handler;
    }

    void SipControllerCore::sipSDPHandler(SipSDPHandler* handler){
        std::lock_guard<std::mutex> lock(m_controllerMutex);
        m_sdpHandler = handler;
    }

    void SipControllerCore::sipCallInfoHandler(SipCallInfoHandler* handler){
        std::lock_guard<std::mutex> lock(m_controllerMutex);
        m_callInfoHandler = handler;
    }

    void SipControllerCore::registerErrorHandler(SipErrorHandler* handler){
        std::lock_guard<std::mutex> lock(m_controllerMutex);
        m_errorHandler = handler;
    }

    void SipControllerCore::sipCallStateHandler(SipCallStateHandler* handler){
        std::lock_guard<std::mutex> lock(m_controllerMutex);
        m_callStateHandler = handler;
    }






/*
 *
 MARK: Custom methods
 *
 */
    Tuple SipControllerCore::processIpAndPort(const SipMessage &message){
        if(message.exists(h_Vias) == false){
            return Tuple();
        }
        Vias::const_iterator it = message.header(h_Vias).end();
        while(true)
        {
            it--;
            if(it->exists(p_received))
            {
                // Check IP from received parameter
                Tuple address(it->param(p_received), 0, UNKNOWN_TRANSPORT);
                if(!address.isPrivateAddress())
                {
                    address.setPort(it->exists(p_rport) ? it->param(p_rport).port() : it->sentPort());
                    address.setType(Tuple::toTransport(it->transport()));
                    return address;
                }
            }
            // Check IP from Via sentHost
            if(DnsUtil::isIpV4Address(it->sentHost())  // Ensure the via host is an IP address (note: web-rtc uses hostnames here instead)
#ifdef USE_IPV6
                || DnsUtil::isIpV6Address(it->sentHost())
#endif
                    )
            {
                Tuple address(it->sentHost(), 0, UNKNOWN_TRANSPORT);
                if(!address.isPrivateAddress())
                {
                    address.setPort(it->exists(p_rport) ? it->param(p_rport).port() : it->sentPort());
                    address.setType(Tuple::toTransport(it->transport()));
                    return address;
                }
            }
            if(it == message.header(h_Vias).begin()) break;
        }
        return Tuple();
    }

    std::string SipControllerCore::getCallId(const SipMessage& msg){
        std::string result;
        if (msg.exists(h_CallId)){
            result = msg.header(h_CallId).value().c_str();
        }
        return result;
    }

    bool SipControllerCore::isValidUserAgent(const SipMessage& msg){
        bool result = false;
        if(msg.exists(h_UserAgent)){
            std::string user_agent = msg.header(h_UserAgent).value().c_str();
            for(char &ch : user_agent){
                ch = std::tolower(ch);
            }
            if (user_agent.find("plivo") != std::string::npos) {
                result = true;
            }
        }
        return  result;
    }

    int SipControllerCore::getStatusCode(const SipMessage& msg){
        int result;
        result = msg.header(h_StatusLine).responseCode();
        return result;
    }

    void SipControllerCore::handleLog(std::string value) {
        SipLogHandler *logHandler;
        {
            std::lock_guard<std::mutex> lock(m_controllerMutex);
            logHandler = m_logHandler;
        }
        logHandler->handleLog("SipControllerCore:: "+value);
    }

    std::string SipControllerCore::createUri(const std::string &username) {
        m_uri = username + "@" + m_domain ;
        std::string sipUri = "<sip:" + m_uri + ";transport=tls>" ;
        return sipUri;
    }

    void SipControllerCore::addapnsInContact(const std::string &certid, std::string &contactAddress, const std::string &token) {
        if ((certid.size() == 0) && (token.size() == 0)){
            contactAddress = contactAddress + ">";
        }

        if (certid.size() != 0){
            contactAddress = contactAddress + ";certid=" + certid + ">";
        }

        if (token.size() != 0){
            std::string p = ">";
            std::string::size_type n = p.length();

            for (std::string::size_type i = contactAddress.find(p);
                 i != std::string::npos;
                 i = contactAddress.find(p))
                contactAddress.erase(i, n);

            contactAddress = contactAddress + ";app_id=" + token + ">";
        }
    }

/// Outgoing call
    void SipControllerCore::sendSdpOffer(std::map<std::string,std::string> headers = {}){
        handleLog("SDP Offer local sdp : " + m_localSdp);

        Data txt(m_localSdp);
        HeaderFieldValue hfv(txt.data(), txt.size());
        Mime type("application", "sdp");
        static SdpContents offerSdp(hfv, type);
        handleLog("send sdp offer 1111");
        SdpContents::Session::Medium* audioMedium = NULL;
        SdpContents::Session::Medium* videoMedium = NULL;

        SdpContents::Session::MediumContainer& mediumContainer = offerSdp.session().media();

        SdpContents::Session::MediumContainer::iterator iter = mediumContainer.begin();
        while (iter != mediumContainer.end()){
            if (iter->name() == "audio")
                audioMedium = &(*iter);
            else if (iter->name() == "video")
                videoMedium = &(*iter);
            iter++;
        }

        std::vector<IceCandidate> localCandidates;
        {
            std::lock_guard<std::mutex> lock(m_controllerMutex);
            localCandidates = m_localCandidates;
        }

        std::vector<IceCandidate>::iterator candidateIterator = localCandidates.begin();
        while (candidateIterator != localCandidates.end()){
            std::string localCandidate = candidateIterator->candidate;
            std::string midString(candidateIterator->mid);
            std::string candidateString(candidateIterator->candidate);
            Data candidateData(candidateString.c_str());
            if (audioMedium != NULL && midString.compare("audio") == 0)
                audioMedium->addAttribute(Data("candidate"), candidateData.substr(10));
            else if (videoMedium != NULL && midString.compare("video") == 0)
                videoMedium->addAttribute(Data("candidate"), candidateData.substr(10));
            candidateIterator++;
        }

        std::string address = "sip:" + m_remoteUri + "@" + m_domain + "";
        SharedPtr<SipMessage> msg = m_userAgent->makeInviteSession(NameAddr(address.c_str()), m_masterProfile, &offerSdp, 0);
        map<string, string>::iterator it;
        for (it = headers.begin(); it != headers.end(); it++){
            const Data headerName(it->first);
            resip::ExtensionHeader h_Tmp(headerName);
            handleLog("SDP Offer header values: " + it->second);
            msg->header(h_Tmp).push_back(StringCategory(it->second.c_str()));
        }
        handleLog("send sdp offer 2222");
        m_userAgent->send(msg);
        handleLog("send sdp offer 3333");
        m_localCandidates.clear();

        SipCallStateHandler *callStateHandler;
        {
            callStateHandler = m_callStateHandler;
        }

        callStateHandler->handleCallState("Calling", 000);
        handleLog("sdp offer sent");
    }

/// Incoming call
    void SipControllerCore::sendSdpAnswer(){
        handleLog("SDP Answer Sent | Incoming call flow");

        Data txt(m_localSdp);

        HeaderFieldValue hfv(txt.data(), txt.size());
        Mime type("application", "sdp");
        static SdpContents answerSdp(hfv, type);

        SdpContents::Session::Medium* audioMedium = NULL;
        SdpContents::Session::Medium* videoMedium = NULL;

        SdpContents::Session::MediumContainer& mediumContainer = answerSdp.session().media();

        SdpContents::Session::MediumContainer::iterator iter = mediumContainer.begin();
        while (iter != mediumContainer.end()){
            if (iter->name() == "audio")
                audioMedium = &(*iter);
            else if (iter->name() == "video")
                videoMedium = &(*iter);
            iter++;
        }

        std::vector<IceCandidate> localCandidates;
        {
            std::lock_guard<std::mutex> lock(m_controllerMutex);
            localCandidates = m_localCandidates;
        }

        std::vector<IceCandidate>::iterator candidateIterator = localCandidates.begin();
        while (candidateIterator != localCandidates.end())
        {
            std::string midString(candidateIterator->mid);
            std::string candidateString(candidateIterator->candidate);
            Data candidateData(candidateString.c_str());
            if (audioMedium != NULL && midString.compare("audio") == 0)
                audioMedium->addAttribute(Data("candidate"), candidateData.substr(10));
            else if (videoMedium != NULL && midString.compare("video") == 0)
                videoMedium->addAttribute(Data("candidate"), candidateData.substr(10));
            candidateIterator++;
        }

        // Call state handler
        SipCallStateHandler *callStateHandler;
        {
            callStateHandler = m_callStateHandler;
        }

        callStateHandler->handleCallState("CallAccepted", 200);

        m_localCandidates.clear();

        m_serverInviteSession->provideAnswer(answerSdp);
        m_serverInviteSession->accept();
    }

    void SipControllerCore::registerUser(std::string username, std::string password){
        isProxyRegister = false;
        std::string sipUri = createUri(username);

        handleLog("registerUser with (username/password): uri : " + sipUri);
#ifndef ANDROID
        Log::setLevel(Log::Stack);
#else
        resip::Log::initialize(resip::Log::Cout, resip::Log::Stack, "SIP", m_androidLog);
#endif
        m_username = username;
        m_password = password;
        setupTransport(sipUri, sipUri);

        setupCredential(username, password);
    }

    void SipControllerCore::registerUser(std::string username, std::string password, std::string token){
        isProxyRegister = false;

        std::string sipUri = createUri(username);

        m_app_id = token;

        std::string contact_url = "<sip:" + m_uri + ";transport=tls;fcm=" + token + ">";

        handleLog("registerUser: uri with (username/password/token): " + sipUri);
#ifndef ANDROID
        Log::setLevel(Log::Stack);
#else
        resip::Log::initialize(resip::Log::Cout, resip::Log::Stack, "SIP", m_androidLog);
#endif
        m_username = username;
        m_password = password;

        setupTransport(sipUri, contact_url);
        setupCredential(username, password);
    }

    void SipControllerCore::registerUser(std::string username, std::string password, const std::string& token, std::string certid){
        isProxyRegister = false;

        std::string sipUri = createUri(username);

        std::string contactAddress = sipUri;

        m_app_id = token;
        m_cert_id = certid;

        addapnsInContact(certid, contactAddress, token);

        handleLog("registerUser: uri with (username/password/token/certid): " + sipUri);

        m_username = username;
        m_password = password;

        setupTransport(sipUri, contactAddress);
        setupCredential(username, password);
    }

    void SipControllerCore::registerUser(std::string username, std::string password, std::string token, std::string certid, std::string proxy, std::map<std::string,std::string> headers){

        isProxyRegister = true;

//        proxy = "phone.plivo.com";
        std::string sipUri = createUri(username);

        std::string contactAddress = sipUri;

        if (m_public_ip_port.size() != 0){
            contactAddress = "<sip:" + username + "@" + m_public_ip_port + ";transport=tls>";
        }

        handleLog("registerUser: uri with (username/password/token/certid/proxy/headers): " + sipUri);
#ifndef ANDROID
        Log::setLevel(Log::Stack);
#else
        resip::Log::initialize(resip::Log::Cout, resip::Log::Stack, "SIP", m_androidLog);
#endif
        if (proxy != "NA"){
            m_proxyServer = proxy;
        }

        m_headers = headers;

        if (certid != "NA"){
            m_cert_id = certid;
        }

        if (token != "NA"){
            m_app_id = token;
        }

        if (certid != "NA" && token != "NA"){
            addapnsInContact(certid, contactAddress, token);
        }else if (certid != "NA"){
            contactAddress = "<sip:" + username + "@" + m_public_ip_port + ";transport=tls;certid=" + certid + ">";
        }else if (token != "NA"){
            contactAddress = "<sip:" + username + "@" + m_public_ip_port + ";transport=tls;fcm=" + token + ">";
        }

        m_username = username;
        m_password = password;

        setupTransport(sipUri, contactAddress);
        setupCredential(username, password);
    }



    void SipControllerCore::setupTransport(std::string address, std::string contact_url){

        Log::setLevel(Log::Stack);
        handleLog("m_useagent before 111");
        //Proxy Setup
        if (m_proxyServer.size() != 0){
            std::string proxyAddress = "sip:" + m_proxyServer + ";transport=tls";
            m_outboundProxy = Uri(Data(proxyAddress));
        }else{
            m_outboundProxy = Uri();
        }

        if (m_stack == nullptr){
            //DNS Setup
            Data dnsServer("8.8.8.8");
            m_dnsServers.push_back(Tuple(dnsServer, 0, UNKNOWN_TRANSPORT).toGenericIPAddress());

            //Certificate and Security setup
            Data caFile(root_cert);

            Security* security;
            security = new Security(caFile);

            //Note: With opensigcom do not remove these line
            //    Compression *compression = new Compression(Compression::DEFLATE);

            security->addCAFile(caFile);
            //    m_stack.reset(new SipStack(security, m_dnsServers,0,false,0,compression));

            m_stack.reset(new SipStack(security, m_dnsServers,0,false,0));
            m_stack->addTransport(UDP,5060);
            m_stack->addTransport(TCP,5060);
            m_stack->addTransport(TLS, 5061);

            m_stack->statisticsManagerEnabled() = false;

            m_userAgent = new DialogUsageManager(*m_stack);

            m_userAgent->setClientRegistrationHandler(this);
            m_userAgent->setInviteSessionHandler(this);

            createMasterProfile();
            m_userAgent->setMasterProfile(m_masterProfile);

            std::auto_ptr<KeepAliveManager> keepAlive(new KeepAliveManager);
            m_userAgent->setKeepAliveManager(keepAlive);

            m_pollGrp = FdPollGrp::create();
            m_interruptor = new EventThreadInterruptor(*m_pollGrp);
            m_stackThread = new EventStackThread(*m_stack, *m_interruptor,*m_pollGrp);

            m_stack->run();
            m_stackThread->run();
        }
        m_clientAuth = std::auto_ptr<ClientAuthManager>(new ClientAuthManager);
        m_userAgent->setClientAuthManager(m_clientAuth);

        m_clientAddress = NameAddr(address.c_str());

        m_masterProfile->setDefaultFrom(m_clientAddress);
        SharedPtr<SipMessage> regMessage = m_userAgent->makeRegistration(m_clientAddress);

        if (m_headers.size() != 0){
            map<string, string>::iterator it;
            for (it = m_headers.begin(); it != m_headers.end(); it++){
                const Data headerName(it->first);
                resip::ExtensionHeader h_Tmp(headerName);

                regMessage->header(h_Tmp).push_back(StringCategory(it->second.c_str()));
            }
        }
        if (contact_url.size() != 0){
            Data contactString(contact_url);
            NameAddr contact(contactString);

            regMessage->header(h_Contacts).pop_back();
            regMessage->header(h_Contacts).push_back(contact);
        }

        regMessage->header(h_Expires).value() = 5184000;
        if (isProxyRegister == true){
            if (!m_outboundProxy.host().empty()){
                regMessage->header(h_Routes).push_back(NameAddr(m_outboundProxy));
            }
        }
        m_userAgent->send(regMessage);

        if (isProxyRegister == false){
            std::lock_guard<std::mutex> lock(m_controllerMutex);
            m_run = true;
            m_receiveClosed = false;
            m_receiver = new std::thread(&SipControllerCore::receive, this);
        }
    }

    void SipControllerCore::setupCredential(std::string username, std::string password){
        m_masterProfile->setDigestCredential(m_clientAddress.uri().host(), m_clientAddress.uri().user(), password.c_str());
    }

    void SipControllerCore::createMasterProfile()
    {
        m_masterProfile = SharedPtr<MasterProfile>(new MasterProfile);
        m_masterProfile->setInstanceId(m_uri.c_str());

        m_masterProfile->clearSupportedMethods();
        m_masterProfile->addSupportedMethod(INVITE);
        m_masterProfile->addSupportedMethod(UPDATE);
        m_masterProfile->addSupportedMethod(ACK);
        m_masterProfile->addSupportedMethod(CANCEL);
        m_masterProfile->addSupportedMethod(OPTIONS);
        m_masterProfile->addSupportedMethod(BYE);
        m_masterProfile->addSupportedMethod(NOTIFY);
        m_masterProfile->addSupportedMethod(SUBSCRIBE);
        m_masterProfile->addSupportedMethod(INFO);
        m_masterProfile->addSupportedMethod(MESSAGE);
        m_masterProfile->addSupportedMethod(PRACK);
        m_masterProfile->setUacReliableProvisionalMode(MasterProfile::Supported);
        m_masterProfile->setUasReliableProvisionalMode(MasterProfile::SupportedEssential);

        // Support Languages
        m_masterProfile->clearSupportedLanguages();
        m_masterProfile->addSupportedLanguage(Token("en"));

        // Support Mime Types
        m_masterProfile->clearSupportedMimeTypes();
        m_masterProfile->addSupportedMimeType(INVITE, Mime("application", "sdp"));
        m_masterProfile->addSupportedMimeType(INVITE, Mime("multipart", "mixed"));
        m_masterProfile->addSupportedMimeType(INVITE, Mime("multipart", "signed"));
        m_masterProfile->addSupportedMimeType(INVITE, Mime("multipart", "alternative"));
        m_masterProfile->addSupportedMimeType(OPTIONS,Mime("application", "sdp"));
        m_masterProfile->addSupportedMimeType(OPTIONS,Mime("multipart", "mixed"));
        m_masterProfile->addSupportedMimeType(OPTIONS, Mime("multipart", "signed"));
        m_masterProfile->addSupportedMimeType(OPTIONS, Mime("multipart", "alternative"));
        m_masterProfile->addSupportedMimeType(PRACK, Mime("application", "sdp"));
        m_masterProfile->addSupportedMimeType(PRACK, Mime("multipart", "mixed"));
        m_masterProfile->addSupportedMimeType(PRACK, Mime("multipart", "signed"));
        m_masterProfile->addSupportedMimeType(PRACK, Mime("multipart", "alternative"));
        m_masterProfile->addSupportedMimeType(UPDATE, Mime("application", "sdp"));
        m_masterProfile->addSupportedMimeType(UPDATE, Mime("multipart", "mixed"));
        m_masterProfile->addSupportedMimeType(UPDATE, Mime("multipart", "signed"));
        m_masterProfile->addSupportedMimeType(UPDATE, Mime("multipart", "alternative"));

        // Supported Options Tags
        m_masterProfile->clearSupportedOptionTags();
        //mMasterProfile->addSupportedOptionTag(Token(Symbols::Replaces));
        m_masterProfile->addSupportedOptionTag(Token(Symbols::Timer));     // Enable Session Timers

        // Supported Schemes
        m_masterProfile->clearSupportedSchemes();
        m_masterProfile->addSupportedScheme("sip");
        m_masterProfile->addSupportedScheme("Digest");

        // Validation Settings
        m_masterProfile->validateContentEnabled() = false;
        m_masterProfile->validateContentLanguageEnabled() = false;
        m_masterProfile->validateAcceptEnabled() = false;

        // Have stack add Allow/Supported/Accept headers to INVITE dialog establishment messages
        m_masterProfile->clearAdvertisedCapabilities(); // Remove Profile Defaults, then add our preferences
        m_masterProfile->addAdvertisedCapability(Headers::Allow);
        //_masterProfile->addAdvertisedCapability(Headers::AcceptEncoding);  // This can be misleading - it might specify what is expected in response
        m_masterProfile->addAdvertisedCapability(Headers::AcceptLanguage);
        m_masterProfile->addAdvertisedCapability(Headers::Supported);
        m_masterProfile->setMethodsParamEnabled(true);

        m_masterProfile->setDefaultRegistrationTime(m_reg_timeout);
        m_masterProfile->setDefaultRegistrationRetryTime(120);

//    if (!m_outboundProxy.host().empty()){
//        m_masterProfile->setOutboundProxy(m_outboundProxy);
//        m_masterProfile->addSupportedOptionTag(Token(Symbols::Outbound));
//    }

        //  m_masterProfile->setUserAgent("PlivoTestIOS-v1.2.0");
        m_masterProfile->setUserAgent("plivo-browser-v1.2.0");
        m_masterProfile->setKeepAliveTimeForDatagram(30);
        m_masterProfile->setKeepAliveTimeForStream(180);
        m_masterProfile->setDefaultStaleCallTime(60);
    }

    void SipControllerCore::registerTimeOut(int time){
        handleLog(&"SipControllerCore m_reg_timeout " [ time] );
        m_reg_timeout = time;
    }

    void SipControllerCore::unregisterUser(){

        m_registrationHandler->handleRegistration(NotRegistered, "Logout");

        handleLog("SipControllerCore logout called");

        m_inInBoundAccepted = false;
        m_inCall = false;
        m_isCaller = false;
        m_localSdpSet = false;
        m_remoteSdpSet = false;
        m_localCandidatesCollected = false;
        m_localSdp.clear();
        m_remoteSdp.clear();
        m_localCandidates.clear();
        m_inInBoundAccepted = false;

        m_receiveMutex.lock();
        m_run = false;
        m_receiveMutex.unlock();

        {
            std::unique_lock<std::mutex> receiveConditionMutex(m_receiveMutex);
            if (!m_receiveClosed)
                m_receiveCondition.wait(receiveConditionMutex);
        }

    }

    void SipControllerCore::createSession(std::string remoteUser, std::string localSdp, std::map<std::string,std::string> headers){
        //Block start
        {
            std::lock_guard<std::mutex> lock(m_controllerMutex);
            m_headers = headers;
            m_isCaller = true;
            m_remoteUri = remoteUser;
        }

        bool localCandidatesCollected;
        {
            std::lock_guard<std::mutex> lock(m_controllerMutex);
            m_localSdp = localSdp;
            m_localSdpSet = true;
            localCandidatesCollected = m_localCandidatesCollected;
        }

        if (localCandidatesCollected)
            sendSdpOffer(headers);
    }









/*
 --------------------------------------------------------------------------------------------------------------------
 *
 *
 * Note: These blocks should run after registration
 MARK: Application layer calling methods
 --------------------------------------------------------------------------------------------------------------------
 */
    void SipControllerCore::acceptSession(std::string localSdp){
        handleLog("acceptSession(localsdp)");

        m_inInBoundAccepted = true;

        bool localCandidatesCollected;
        {
            std::lock_guard<std::mutex> lock(m_controllerMutex);
            m_localSdp = localSdp;
            m_localSdpSet = true;
            localCandidatesCollected = m_localCandidatesCollected;
        }

        if (localCandidatesCollected){
            sendSdpAnswer();
            std::lock_guard<std::mutex> lock(m_controllerMutex);
            m_inCall = true;
        }
    }

/// Call got rejected from UAC side. Make sure now onTerminated is not getting called
/// Note : It can lead to app crash!
    void SipControllerCore::reject(){
        handleLog("Rejecting call in SIP Controle layer");

        isUACRejected = true;

        if (m_serverInviteSession){
            m_serverInviteSession->reject(486);

            m_inInBoundAccepted = false;
            m_isCaller = false;
            m_localSdpSet = false;
            m_remoteSdpSet = false;
            m_localCandidatesCollected = false;
            m_localSdp.clear();
            m_remoteSdp.clear();
            m_localCandidates.clear();
            m_current_callid.clear();
        }
    }

    void SipControllerCore::networkChange(){
        handleLog("reINVITE sending...");
        std::string address = "sip:" + m_remoteUri + "@" + m_domain + "";
        if (m_clientInviteSession){
            m_clientInviteSession->targetRefresh(NameAddr(address.c_str()));
        }

        if (m_serverInviteSession){
            m_serverInviteSession->targetRefresh(NameAddr(address.c_str()));
        }
    }

    void SipControllerCore::ringing(){
        handleLog("Sending ringing 180 in SIP Controle layer");
        if (m_serverInviteSession){
            m_serverInviteSession->provisional();
        }
    }

    void SipControllerCore::onIceCandidate(std::string &sdp, std::string &mid){
        handleLog("onIceCandidate");

        {
            IceCandidate iceCandidate;
            iceCandidate.mid = mid;
            iceCandidate.candidate = sdp;
            m_localCandidates.push_back(iceCandidate);
        }
    }

    void SipControllerCore::onIceGatheringFinished(){

        handleLog("onIceGatheringFinished");

        bool isCaller;
        bool localSdpSet;
        {
            std::lock_guard<std::mutex> lock(m_controllerMutex);
            isCaller = m_isCaller;
            localSdpSet = m_localSdpSet;
        }

        if (isCaller && localSdpSet){
//            if (!m_headers.empty()){
//                sendSdpOffer(m_headers);
//            }else{
                sendSdpOffer();
//            }

            {
                std::lock_guard<std::mutex> lock(m_controllerMutex);
                m_inCall = true;
            }

        } else if (!isCaller && localSdpSet && m_inInBoundAccepted){
            handleLog("onIceGatheringFinished sending answer");
            sendSdpAnswer();

            {
                std::lock_guard<std::mutex> lock(m_controllerMutex);
                m_inCall = true;
            }
        }

        {
            std::lock_guard<std::mutex> lock(m_controllerMutex);
            m_localCandidatesCollected = true;
        }
    }

    void SipControllerCore::terminateSession(bool destroyLocalStream){

        handleLog("terminateSession requested from application layer");

        if (mProgress == Done){
            handleLog("terminateSession() already destroyed due to progress == done | returing from here");
            return;
        }

        bool inCall;
        {
            std::lock_guard<std::mutex> lock(m_controllerMutex);
            inCall = m_inCall;
        }

        {
            std::lock_guard<std::mutex> lock(m_controllerMutex);
            m_inCall = false;
        }

        if (m_clientInviteSession)
            m_clientInviteSession->end(InviteSession::UserHangup);
        else if (m_serverInviteSession)
            m_serverInviteSession->end(InviteSession::UserHangup);

        std::lock_guard<std::mutex> lock(m_controllerMutex);
        m_inInBoundAccepted = false;
        m_isCaller = false;
        m_localSdpSet = false;
        m_remoteSdpSet = false;
        m_localCandidatesCollected = false;
        m_localSdp.clear();
        m_remoteSdp.clear();
        m_localCandidates.clear();
        m_current_callid.clear();

        if (m_clientInviteSession){
            m_clientInviteSession.reset();
        }

        if (m_serverInviteSession){
            m_serverInviteSession.reset();
        }
    }







/*
 --------------------------------------------------------------------------------------------------------------------
 *
 *
 *These blocks should run after registration
 MARK: InviteSessionHandle
 *
 --------------------------------------------------------------------------------------------------------------------
 */

/// called when an dialog enters the terminated state - this can happen
/// after getting a BYE, Cancel, or 4xx,5xx,6xx response - or the session
/// times out
    void SipControllerCore::onTerminated(InviteSessionHandle, InviteSessionHandler::TerminatedReason reason, const SipMessage* msg){
        mProgress = Done;
        handleLog("onTerminated() terminate from the same callid ");

        SipCallStateHandler *callStateHandler;
        bool inCall;

        {
            std::lock_guard<std::mutex> lock(m_controllerMutex);
            callStateHandler = m_callStateHandler;
        }

        SipCallHandler *callHandler;
        {
            std::lock_guard<std::mutex> lock(m_controllerMutex);
            callHandler = m_callHandler;
            inCall = m_inCall;
        }



        if (isUACRejected){
            callHandler->handleCall(TerminateCall, "Local call rejected");
            sleep(2);
            m_serverInviteSession.reset();
            handleLog("onTerminated() iSUACRejected = true | returing from here");
            return;
        }

        std::string callid = msg->header(h_CallId).value().c_str();


        //Some basic reset
        Data reasonData;
        std::string reasonDataString;
        unsigned int statusCode = 603;

        switch(reason)
        {
            case InviteSessionHandler::RemoteBye:
                reasonData = "received a BYE from peer";
                reasonDataString = "RemoteBye";
                statusCode = 200;//Making it 200 bcz i m expecting call terminated after successfully connected
                break;
            case InviteSessionHandler::RemoteCancel:
                reasonData = "received a CANCEL from peer";
                reasonDataString = "RemoteCancel";
                statusCode = 487;
                break;
            case InviteSessionHandler::Rejected:
                reasonData = "received a rejection from peer";
                reasonDataString = "Rejected";
                statusCode = 486;
                break;
            case InviteSessionHandler::LocalBye:
                reasonData = "ended locally via BYE";
                reasonDataString = "LocalBye";
                break;
            case InviteSessionHandler::LocalCancel:
                reasonData = "ended locally via CANCEL";
                reasonDataString = "LocalCancel";
                break;
            case InviteSessionHandler::Replaced:
                reasonData = "ended due to being replaced";
                reasonDataString = "Replaced";
                break;
            case InviteSessionHandler::Referred:
                reasonData = "ended due to being reffered";
                reasonDataString = "Referred";
                break;
            case InviteSessionHandler::Error:
                reasonData = "ended due to an error";
                reasonDataString = "Error";
                break;
            case InviteSessionHandler::Timeout:
                reasonData = "ended due to a timeout";
                reasonDataString = "Timeout";
                break;
            default:
                reasonData = "default error";
                reasonDataString = "Default Error";
                break;
        }


        // 486 - Busy
        // 487 - Cancelled
        if(msg){
            if(msg->isResponse()){
                statusCode = msg->header(h_StatusLine).responseCode();
            }
        }

        if(msg){
            InfoLog(<< "InviteSessionHandle onTerminated: status code = " << statusCode <<" reason= " << reasonData << ", msg=" << msg->brief());
        }else{
            InfoLog(<< "InviteSessionHandle onTerminated: reason=" << reasonData);
        }

        callStateHandler->handleCallState(reasonDataString, statusCode);

        if (callHandler)
            callHandler->handleCall(TerminateCall, reasonDataString);

        if (!inCall){
            handleLog("onTerminated() already destroyed | returing from here");
            return;
        }

        m_inInBoundAccepted = false;

        m_current_callid.clear();
        m_inCall = false;
        m_isCaller = false;
        m_localSdpSet = false;
        m_remoteSdpSet = false;
        m_localCandidatesCollected = false;
        m_localSdp.clear();
        m_remoteSdp.clear();
        m_localCandidates.clear();

        m_clientInviteSession.reset();
        m_serverInviteSession.reset();
    }

/// Outgoing call - called when an answer is received - has nothing to do with user
/// answering the call
    void SipControllerCore::onAnswer(InviteSessionHandle, const SipMessage& msg, const SdpContents& sdp){
        handleLog("InviteSessionHandle Answer received | Outgoing call flow");

        SipCallHandler *callHandler;
        {
            std::lock_guard<std::mutex> lock(m_controllerMutex);
            callHandler = m_callHandler;
        }

        SipCallStateHandler *callStateHandler;
        {
            std::lock_guard<std::mutex> lock(m_controllerMutex);
            callStateHandler = m_callStateHandler;
        }

        callStateHandler->handleCallState("CallAccepted", msg.header(h_StatusLine).responseCode());

        SipSDPHandler *sdpHandler;
        {
            std::lock_guard<std::mutex> lock(m_controllerMutex);
            sdpHandler = m_sdpHandler;
        }

        HeaderFieldValue headerFieldValue = msg.getRawBody();
        std::string remoteSdp = sdp.getBodyData().c_str();
        handleLog("SDP Answer | outgoing call flow remote SDP : "+remoteSdp);

        std::string callid = getCallId(msg);
        m_current_callid = callid;

        std::string callInfo = "call_id:" + callid;

        callInfo = callInfo + "," + "type:" + "outgoing";

        for (auto &&i :msg.getRawUnknownHeaders()){
            std::string value = msg.header(ExtensionHeader(i.first)).front().value().c_str();
            callInfo = callInfo + "," + i.first.c_str() + ":" + value;
        }

        //Append extra headers also in the same info
        //Call info handler
        SipCallInfoHandler *callInfoHandler;
        {
            std::lock_guard<std::mutex> lock(m_controllerMutex);
            callInfoHandler = m_callInfoHandler;
        }

        callInfoHandler->handleCallInfo(callInfo);

//        sdpHandler->handleSDP(remoteSdp);

        {
            std::lock_guard<std::mutex> lock(m_controllerMutex);
            m_inCall = true;
            m_remoteSdp = remoteSdp;
            m_remoteSdpSet = true;
        }

        if (callHandler)
            callHandler->handleCall(CallAccepted, "");
    }


/// Incoming call - called when an offer is received - must send an answer soon after this
    void SipControllerCore::onOffer(InviteSessionHandle, const SipMessage& msg, const SdpContents& sdp){
        handleLog("InviteSessionHandle Offer received | Incoming call flow");

        SipCallInfoHandler *callInfoHandler;
        SipCallStateHandler *callStateHandler;
        SipCallHandler *callHandler;
        SipSDPHandler *sdpHandler;

        resip::H_From headerType;
        H_From::Type from = msg.header(headerType);
        const char* fromC = from.uri().user().c_str();

        resip::H_To toHeaderType;
        H_To::Type to = msg.header(toHeaderType);
        const char* toC = to.uri().user().c_str();

        std::string callid = getCallId(msg);
        m_current_callid = callid;

        std::string callInfo = "call_id:" + callid;

        //Add to contact to the info
        callInfo = callInfo + "," + "to:" + toC;

        //Add to contact to the info
        callInfo = callInfo + "," + "from:" + fromC;

//    callInfo = callInfo + "," + "type:" + "incoming";

        for (auto &&i :msg.getRawUnknownHeaders()){
            std::string value = msg.header(ExtensionHeader(i.first)).front().value().c_str();
            callInfo = callInfo + "," + i.first.c_str() + ":" + value;
        }

        ///Call Info Handler
        {
            std::lock_guard<std::mutex> lock(m_controllerMutex);
            callInfoHandler = m_callInfoHandler;
        }
        callInfoHandler->handleCallInfo(callInfo);


        ///SDP Handler
        {
            std::lock_guard<std::mutex> lock(m_controllerMutex);
            sdpHandler = m_sdpHandler;
        }
        HeaderFieldValue headerFieldValue = msg.getRawBody();
        std::string remoteSdp = sdp.getBodyData().c_str();
        handleLog("SDP Offer incoming \n " + remoteSdp);
        sdpHandler->handleSDP(remoteSdp);

        ///Call Handler
        {
            std::lock_guard<std::mutex> lock(m_controllerMutex);
            callHandler = m_callHandler;
        }
        if (callHandler){
            callHandler->handleCall(IncomingCall, fromC);
        }

        ///Call State Handler
        {
            std::lock_guard<std::mutex> lock(m_controllerMutex);
            callStateHandler = m_callStateHandler;
        }
        callStateHandler->handleCallState("EarlyMedia", 183);

        {
            std::lock_guard<std::mutex> lock(m_controllerMutex);
            m_isCaller = false;
            m_inCall = true;
            m_remoteSdp = remoteSdp;
            m_remoteSdpSet = true;
        }
    }

/// called when a dialog initiated as a UAS enters the connected state
    void SipControllerCore::onConnected(InviteSessionHandle, const SipMessage& msg){
        handleLog("InviteSessionHandle Session connected");
    }

/// called when an Invite w/out offer is sent, or any other context which
/// requires an offer from the user
    void SipControllerCore::onOfferRequired(InviteSessionHandle, const SipMessage& msg){
        handleLog("InviteSessionHandle onOfferRequired");
    }

/// called if an offer in a UPDATE or re-INVITE was rejected - not real
/// useful. A SipMessage is provided if one is available
    void SipControllerCore::onOfferRejected(InviteSessionHandle, const SipMessage* msg){
        handleLog("InviteSessionHandle onOfferRejected");
    }

/// called when INFO message is received
/// the application must call acceptNIT() or rejectNIT()
/// once it is ready for another message.
    void SipControllerCore::onInfo(InviteSessionHandle, const SipMessage& msg){
        handleLog("InviteSessionHandle onInfo");
    }

/// called when response to INFO message is received
    void SipControllerCore::onInfoSuccess(InviteSessionHandle, const SipMessage& msg){
        handleLog("InviteSessionHandle onInfoSuccess");
    }

    void SipControllerCore::onInfoFailure(InviteSessionHandle, const SipMessage& msg){
        handleLog("InviteSessionHandle onInfoFailure");
    }

/// called when MESSAGE message is received
    void SipControllerCore::onMessage(InviteSessionHandle, const SipMessage& msg){
        handleLog("InviteSessionHandle onMessage");
    }

/// called when response to MESSAGE message is received
    void SipControllerCore::onMessageSuccess(InviteSessionHandle, const SipMessage& msg){
        handleLog("InviteSessionHandle onMessageSuccess");
    }

    void SipControllerCore::onMessageFailure(InviteSessionHandle, const SipMessage& msg){
        handleLog("InviteSessionHandle onMessageFailure");
    }

/// called when an REFER message is received.  The refer is accepted or
/// rejected using the server subscription. If the offer is accepted,
/// DialogUsageManager::makeInviteSessionFromRefer can be used to create an
/// InviteSession that will send notify messages using the ServerSubscription
    void SipControllerCore::onRefer(InviteSessionHandle, ServerSubscriptionHandle, const SipMessage& msg){
        handleLog("InviteSessionHandle onRefer");
    }

    void SipControllerCore::onReferNoSub(InviteSessionHandle, const SipMessage& msg){
        handleLog("InviteSessionHandle onReferNoSub");
    }

/// called when an REFER message receives a failure response
    void SipControllerCore::onReferRejected(InviteSessionHandle, const SipMessage& msg){
        handleLog("InviteSessionHandle onReferRejected");
    }

/// called when an REFER message receives an accepted response
    void SipControllerCore::onReferAccepted(InviteSessionHandle, ClientSubscriptionHandle, const SipMessage& msg){
        handleLog("InviteSessionHandle onReferAccepted");
    }





/*
 --------------------------------------------------------------------------------------------------------------------
 *
 *
 *These blocks should run after registration
 MARK: Register ClientRegistrationHandle
 *
 *
 *
 --------------------------------------------------------------------------------------------------------------------
 */
/// Called when registraion succeeds or each time it is sucessfully
/// refreshed (manual refreshes only).
    void SipControllerCore::onSuccess(ClientRegistrationHandle, const SipMessage& response){
        resip::H_From headerType;
        H_From::Type from = response.header(headerType);
        Data uri = from.uri().getAor();
        const char* fromC = from.uri().user().c_str();

        //TODO:Give it back to where it required
        std::string callid = response.header(h_CallId).value().c_str();
        std::string callInfo = "call_id:" + callid;
        handleLog("onSuccess: call info 1111 : " + callInfo);
        if (true){
            SipRegistrationHandler *registrationHandler;
            {
                std::lock_guard<std::mutex> lock(m_controllerMutex);
                registrationHandler = m_registrationHandler;
            }

            SipCallInfoHandler *callInfoHandler;
            {
                std::lock_guard<std::mutex> lock(m_controllerMutex);
                callInfoHandler = m_callInfoHandler;
            }

            callInfoHandler->handleCallInfo(callInfo);

            handleLog("onSuccess: call info : " + callInfo);
            registrationHandler->handleRegistration(Registered, fromC);
        }

        isProxyRegister = false;

        if (response.header(h_Vias).empty() == false){
            Tuple publicAddress = processIpAndPort(response);

            if(publicAddress.getType() != UNKNOWN_TRANSPORT)
            {
                std::string ip = Tuple::inet_ntop(publicAddress).c_str();
                std::string port = patch::to_string(publicAddress.getPort());
                if (ip.size() != 0 && port.size() != 0){
                    m_public_ip_port = ip + ":" + port;
                    handleLog("resolving ip and port "  + ip + port);
                }
            }
        }
    }

/// Called when all of my bindings have been removed
    void SipControllerCore::onRemoved(ClientRegistrationHandle, const SipMessage& response)
    {
        handleLog("ClientRegistrationHandle onRemoved");
    }

/// From resip/dum/RegistrationHandler.hxx
/// call on Retry-After failure.
/// return values: -1 = fail, 0 = retry immediately, N = retry in N seconds
    int SipControllerCore::onRequestRetry(ClientRegistrationHandle, int retrySeconds, const SipMessage& response)
    {
        handleLog("ClientRegistrationHandle onRequestRetry");
        return 0;
    }

/// Called if registration fails, usage will be destroyed (unless a
/// Registration retry interval is enabled in the Profile)
    void SipControllerCore::onFailure(ClientRegistrationHandle, const SipMessage& response)
    {
        handleLog("ClientRegistrationHandle onFailure" );
    }

/// Called when a TCP or TLS flow to the server has terminated.  This can be caused by socket
/// errors, or missing CRLF keep alives pong responses from the server.
// Called only if clientOutbound is enabled on the UserProfile and the first hop server
/// supports RFC5626 (outbound).
/// Default implementation is to immediately re-Register in an attempt to form a new flow.
    void SipControllerCore:: onFlowTerminated(ClientRegistrationHandle){

    }





/*
 --------------------------------------------------------------------------------------------------------------------
 *
 * Call flow handlers method from reSIProcate
 * These blocks should run after registration.
 MARK: Call flow | InviteSessionHandle
 *
 --------------------------------------------------------------------------------------------------------------------
 */
/// called when an initial INVITE or the intial response to an outgoing invite
    void SipControllerCore::onNewSession(ServerInviteSessionHandle sis, InviteSession::OfferAnswerType oat, const SipMessage& msg){
        mProgress = Ringing;
        isUACRejected = false;
        handleLog("Incoming call.....onNewSession()");

        if (isValidUserAgent(msg)){
            {
                std::lock_guard<std::mutex> lock(m_controllerMutex);
                m_serverInviteSession = SharedPtr<ServerInviteSession>(sis.get());
            }
        }else{
            handleLog("ServerInviteSessionHandle onNewSession: Error Incoming call from invalid user agent");
        }
    }

/// called when an initial INVITE or the intial response to an outgoing invite
    void SipControllerCore::onNewSession(ClientInviteSessionHandle cis, InviteSession::OfferAnswerType oat, const SipMessage& msg){
        mProgress = Dialing;
        handleLog("Outgoing call.....onNewSession()");
        isUACRejected = false;

        if (isValidUserAgent(msg)){
            {
                std::lock_guard<std::mutex> lock(m_controllerMutex);
                m_clientInviteSession = SharedPtr<ClientInviteSession>(cis.get());
            }
        }else{
            handleLog("ClientInviteSessionHandle onNewSession: Error Outgoing call not a valid user agent");
        }

        SipSDPHandler *sdpHandler;
        {
            std::lock_guard<std::mutex> lock(m_controllerMutex);
            sdpHandler = m_sdpHandler;
        }
        sdpHandler->handleSDP(msg.getContents()->getBodyData().c_str());
    }

/// Received a failure response from UAS
    void SipControllerCore::onFailure(ClientInviteSessionHandle, const SipMessage& msg){
        mProgress = Done;
        handleLog("ClientInviteSessionHandle onFailure: Unhandled method invoked ");
    }

/// called when an in-dialog provisional response is received that contains a body | Note: To handle ringing state
    void SipControllerCore::onEarlyMedia(ClientInviteSessionHandle, const SipMessage& msg, const SdpContents& sdp){
        handleLog("ClientInviteSessionHandle onEarlyMedia");
        mProgress = Dialing;

        m_current_callid = getCallId(msg);

        std::string callInfo = "call_id:" + m_current_callid;

        for (auto &&i :msg.getRawUnknownHeaders()){
            std::string value = msg.header(ExtensionHeader(i.first)).front().value().c_str();
            callInfo = callInfo + "," + i.first.c_str() + ":" + value;
        }

        SipCallInfoHandler *callInfoHandler;
        {
            std::lock_guard<std::mutex> lock(m_controllerMutex);
            callInfoHandler = m_callInfoHandler;
        }
        callInfoHandler->handleCallInfo(callInfo);

        SipCallStateHandler *callStateHandler;
        {
            std::lock_guard<std::mutex> lock(m_controllerMutex);
            callStateHandler = m_callStateHandler;
        }
        callStateHandler->handleCallState("EarlyMedia", getStatusCode(msg));
    }

/// called when dialog enters the Early state - typically after getting 18x
    void SipControllerCore::onProvisional(ClientInviteSessionHandle, const SipMessage& msg){
        mProgress = Dialing;
        handleLog("ClientInviteSessionHandle onProvisional");
    }

/// called when a dialog initiated as a UAC enters the connected state
    void SipControllerCore::onConnected(ClientInviteSessionHandle cis, const SipMessage& msg){
        mProgress = Connected;
        handleLog("ClientInviteSessionHandle Session connected");
    }

/// called when a fork that was created through a 1xx never receives a 2xx
/// because another fork answered and this fork was canceled by a proxy.
    void SipControllerCore::onForkDestroyed(ClientInviteSessionHandle){
        mProgress = Done;
        handleLog("ClientInviteSessionHandle onForkDestroyed");
    }

/** UAC gets no final response within the stale call timeout (default is 3
 * minutes). This is just a notification. After the notification is
 * called, the InviteSession will then call
 * InviteSessionHandler::terminate() */
    void SipControllerCore::onStaleCallTimeout(ClientInviteSessionHandle){
        mProgress = Done;
        handleLog("ClientInviteSessionHandle onStaleCallTimeout");
    }

/// called when a 3xx with valid targets is encountered in an early dialog
/// This is different then getting a 3xx in onTerminated, as another
/// request will be attempted, so the DialogSet will not be destroyed.
/// Basically an onTermintated that conveys more information.
/// checking for 3xx respones in onTerminated will not work as there may
/// be no valid targets.
    void SipControllerCore::onRedirected(ClientInviteSessionHandle, const SipMessage& msg){
        mProgress = Done;
        handleLog("ClientInviteSessionHandle onRedirected");
    }

}
