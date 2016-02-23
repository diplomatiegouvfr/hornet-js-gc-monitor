// v8 compatibility templates

#include "hornet-js-gc-monitor.h"

using namespace std;
using namespace v8;

namespace hornet {

// our singleton instance
GCMonitor* GCMonitor::instance_ = NULL;

// some utility functions to make code cleaner
static inline v8::Local<v8::String> v8_str( const char* s ) {
    Nan::MaybeLocal<v8::String> s_maybe = Nan::New<v8::String>(s);
    return s_maybe.ToLocalChecked();
}

static inline v8::Local<v8::Value> getObjectProperty(const v8::Local<v8::Object>& object,
                                                     const char* key) {
    v8::Local<v8::String> keyString = v8_str(key);
    Nan::MaybeLocal<v8::Value> value = Nan::Get(object, keyString);
    if (value.IsEmpty()) {
        return Nan::Undefined();
    }
    return value.ToLocalChecked();
}

/**
 * obtain reference to process.gcMonitor from global object
 *
 * Preconditions:  process.gcMonitor exists and is an object
 **/
static v8::Local<v8::Object> getProcessGCMonitorObject() {
    Nan::EscapableHandleScope scope;

    v8::Local<v8::Object> global = Nan::GetCurrentContext()->Global();
    v8::Local<v8::Value> process = getObjectProperty( global, "process" );
    assert( Nan::Undefined() != process && process->IsObject());

    // the object must not exists !!
    v8::Local<v8::Value> gcMonitorValue = getObjectProperty(process.As<v8::Object>(), GCMonitor::getJsObjectName() );
    assert(Nan::Undefined() == gcMonitorValue);

    v8::Local<v8::Object> gcMonitor = Nan::New<v8::Object>();
    Nan::Set( process.As<v8::Object>(), v8_str(GCMonitor::getJsObjectName()), gcMonitor );

    return scope.Escape(gcMonitor.As<v8::Object>());
}

const GCStat GCUsage::getGCStat() {
    return stats_;
}

static NAN_GETTER(GetterGCTotalCount) {
    GCMonitor& monitor = GCMonitor::getInstance();
    GCUsageTracker& tracker = monitor.getGCUsageTracker();

    info.GetReturnValue().Set(Nan::New<Number>(tracker.totalCollections()));
}

static NAN_GETTER(GetterGCTotalTime) {
    GCMonitor& monitor = GCMonitor::getInstance();
    GCUsageTracker& tracker = monitor.getGCUsageTracker();

    info.GetReturnValue().Set(Nan::New<Number>(tracker.totalElapsedTime() / (1000.0 * 1000.0) ));
}

static NAN_GETTER(GetterGCScavengeCount) {
    GCMonitor& monitor = GCMonitor::getInstance();
    GCUsageTracker& tracker = monitor.getGCUsageTracker();
    GCUsage& scavengeTracker = tracker.GetGCUsage(GCUsageTracker::indexTov8GCType(0));
    const GCStat& scavengeStat = scavengeTracker.getGCStat();

    info.GetReturnValue().Set(Nan::New<Number>(scavengeStat.numCalls));
}

static NAN_GETTER(GetterGCMarksweepCount) {
    GCMonitor& monitor = GCMonitor::getInstance();
    GCUsageTracker& tracker = monitor.getGCUsageTracker();
    GCUsage& marksweepTracker = tracker.GetGCUsage(GCUsageTracker::indexTov8GCType(1));
    const GCStat& marksweepStat = marksweepTracker.getGCStat();

    info.GetReturnValue().Set(Nan::New<Number>(marksweepStat.numCalls));
}

static NAN_GETTER(GetterGCScavengeTime) {
    GCMonitor& monitor = GCMonitor::getInstance();
    GCUsageTracker& tracker = monitor.getGCUsageTracker();
    GCUsage& scavengeTracker = tracker.GetGCUsage(GCUsageTracker::indexTov8GCType(0));
    const GCStat& scavengeStat = scavengeTracker.getGCStat();

    info.GetReturnValue().Set(Nan::New<Number>(scavengeStat.cumulativeTime / (1000.0 * 1000.0) ));
}

static NAN_GETTER(GetterGCMarksweepTime) {
    GCMonitor& monitor = GCMonitor::getInstance();
    GCUsageTracker& tracker = monitor.getGCUsageTracker();
    GCUsage& marksweepTracker = tracker.GetGCUsage(GCUsageTracker::indexTov8GCType(1));
    const GCStat& marksweepStat = marksweepTracker.getGCStat();

    info.GetReturnValue().Set(Nan::New<Number>(marksweepStat.cumulativeTime / (1000.0 * 1000.0) ));
}

static NAN_GETTER(GetterGCScavengeMaxTime) {
    GCMonitor& monitor = GCMonitor::getInstance();
    GCUsageTracker& tracker = monitor.getGCUsageTracker();
    GCUsage& scavengeTracker = tracker.GetGCUsage(GCUsageTracker::indexTov8GCType(0));
    const GCStat& scavengeStat = scavengeTracker.getGCStat();

    info.GetReturnValue().Set(Nan::New<Number>(scavengeStat.maxTime / (1000.0 * 1000.0) ));
}

static NAN_GETTER(GetterGCMarksweepMaxTime) {
    GCMonitor& monitor = GCMonitor::getInstance();
    GCUsageTracker& tracker = monitor.getGCUsageTracker();
    GCUsage& marksweepTracker = tracker.GetGCUsage(GCUsageTracker::indexTov8GCType(1));
    const GCStat& marksweepStat = marksweepTracker.getGCStat();

    info.GetReturnValue().Set(Nan::New<Number>(marksweepStat.maxTime / (1000.0 * 1000.0) ));
}

static NAN_GC_CALLBACK(startGC) {
    GCMonitor& monitor = GCMonitor::getInstance();
    GCUsageTracker& tracker = monitor.getGCUsageTracker();
    tracker.StartGC(type);
}

static NAN_GC_CALLBACK(stopGC) {
    GCMonitor& monitor = GCMonitor::getInstance();
    GCUsageTracker& tracker = monitor.getGCUsageTracker();
    tracker.StopGC(type);
}

static void InstallGCEventCallbacks() {
    Nan::AddGCPrologueCallback(startGC);
    Nan::AddGCEpilogueCallback(stopGC);
}

static void UninstallGCEventCallbacks() {
    Nan::RemoveGCPrologueCallback(startGC);
    Nan::RemoveGCEpilogueCallback(stopGC);
}

/**
 * Set up the singleton instance variable
 */
void GCMonitor::Initialize(v8::Isolate* isolate) {
    if (instance_) return;

    assert(0 != isolate);
    instance_ = new GCMonitor(isolate);
}


/**
 * Set up process.gcMonitor object and accessors for count/elapsed
 * These can be read from Javascript user code space
 **/
void GCMonitor::InitializeProcessObject() {
    Nan::HandleScope scope;

    v8::Local<v8::Object> gcObj = getProcessGCMonitorObject();

    {
        v8::Local<v8::Object> totalObj = Nan::New<v8::Object>();
        Nan::Set( gcObj, v8_str("total"), totalObj );
        Nan::SetAccessor( totalObj, v8_str("count"), GetterGCTotalCount, 0 );
        Nan::SetAccessor( totalObj, v8_str("time"), GetterGCTotalTime, 0 );

        v8::Local<v8::Object> scavengeObj = Nan::New<v8::Object>();
        Nan::Set( gcObj, v8_str("scavenge"), scavengeObj );
        Nan::SetAccessor( scavengeObj, v8_str("count"), GetterGCScavengeCount, 0 );
        Nan::SetAccessor( scavengeObj, v8_str("time"), GetterGCScavengeTime, 0 );
        Nan::SetAccessor( scavengeObj, v8_str("maxtime"), GetterGCScavengeMaxTime, 0 );

        v8::Local<v8::Object> marksweepObj = Nan::New<v8::Object>();
        Nan::Set( gcObj, v8_str("marksweep"), marksweepObj );
        Nan::SetAccessor( marksweepObj, v8_str("count"), GetterGCMarksweepCount, 0 );
        Nan::SetAccessor( marksweepObj, v8_str("time"), GetterGCMarksweepTime, 0 );
        Nan::SetAccessor( marksweepObj, v8_str("maxtime"), GetterGCMarksweepMaxTime, 0 );
    }
}

GCUsage::GCUsage() {
    int rc = uv_mutex_init(&lock_);
    if (0 != rc) perror("GCUsage: could not initialize uv_mutex");

    bzero( &stats_, sizeof(GCStat) );
    startTime_ = 0;
}

GCUsage::~GCUsage() {
    uv_mutex_destroy(&lock_);
}

/** Start() is called at the start of a GC event.
 *
 *  It runs inside the v8 isolate, so it is able to make calls to
 *  Javascript functions, etc if required
 **/
void GCUsage::Start() {
    startTime_ = uv_hrtime();
}

/** Stop() is called at the end of a GC event.
 *
 *  It runs inside the v8 isolate, so it is able to make
 *  calls to Javascript functions, etc if required
 **/
uint64_t GCUsage::Stop() {
    assert(0 != startTime_);
    uint64_t elapsed = uv_hrtime() - startTime_;
    {
        // We need this lock to prevent the profiling monitr
        // thread from potentially clearing the stats_ instance
        // variable at the same time as we update it
        ScopedUVLock scope( &lock_ );

        stats_.numCalls++;
        stats_.cumulativeTime += elapsed;
        if (elapsed > stats_.maxTime) {
            stats_.maxTime = elapsed;
        }
    }
    return elapsed;
}

GCMonitor& GCMonitor::getInstance() {
    assert( 0 != instance_ );
    return *instance_;
}

/**
 * Activate the monitor along with any required initialization
 */
void GCMonitor::Start(v8::Local<v8::Value> arg) {

    assert( 0 != instance_ );

    if ( running_ ) return;
    running_ = true;

    if (Nan::Undefined() != arg) {
        v8::String::Utf8Value argString(arg->ToString());
        std::string jsObjectName = std::string(*argString);

        jsObjectName_ = jsObjectName.c_str();
    } else {
        jsObjectName_ = "gcMonitor";
    }

    InitializeProcessObject();
    InstallGCEventCallbacks();
}

/**
 * Stop monitoring
 *
 * Uninstall any callbacks for GC, stop the thread and close socket
 */
void GCMonitor::Stop() {
    assert( 0 != instance_ );

    UninstallGCEventCallbacks();

    running_ = false;
}

/**
 * Create a new monitor instance based on the v8 isolate
 */
GCMonitor::GCMonitor(v8::Isolate* isolate) :
    running_(false),
    gcTracker_(),
    isolate_(isolate) {
}

GCMonitor::~GCMonitor() {
}

static NAN_METHOD(StartMonitor) {
    GCMonitor::getInstance().Start(info[0]);
    info.GetReturnValue().SetUndefined();
}

static NAN_METHOD(StopMonitor) {
    GCMonitor::getInstance().Stop();
    info.GetReturnValue().SetUndefined();
}


// main module initialization
NAN_MODULE_INIT(init) {
    // target is defined in the NAN_MODULE_INIT macro as ADDON_REGISTER_FUNCTION_ARGS_TYPE
    //  -- i.e. v8::Local<v8::Object> for nodejs-3.x and up,
    // but v8::Handle<v8::Object> for nodejs-0.12.0

#if NODE_MODULE_VERSION < IOJS_3_0_MODULE_VERSION
    Local<Object> exports = Nan::New<Object>(target);
#else
    Local<Object> exports = target;
#endif

    Nan::Export( exports, "start", StartMonitor);
    Nan::Export( exports, "stop", StopMonitor);

    GCMonitor::Initialize(v8::Isolate::GetCurrent());
}

NODE_MODULE(hornet_js_gc_monitor, init)
}
