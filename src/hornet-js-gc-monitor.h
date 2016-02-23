#ifndef GC_MONITOR_H_
#define GC_MONITOR_H_

#include "nan.h"
#include <stdint.h>
#include <assert.h>

namespace hornet {


/** POD structure that contains GC statistics for a given "interval"
 *  Times are in nanoseconds
 */
typedef struct {
    long unsigned int numCalls;
    uint64_t cumulativeTime; //< total time of all collections (ns)
    uint64_t maxTime;        //< max time of any single GC (ns)
} GCStat;

/**
 * Simple wrapper class to handle scoped lock for pre-existing libuv mutex
 */
class ScopedUVLock {
 public:
    ScopedUVLock(uv_mutex_t* m) : m_(m) { uv_mutex_lock(m_); }
    ~ScopedUVLock() { uv_mutex_unlock( m_ ); }
 private:
    uv_mutex_t* m_;

    ScopedUVLock();  //< can't create one of these without a reference to a mutex
};

class GCUsage {
    public:
        GCUsage();
        ~GCUsage();

        const GCStat getGCStat();

        /**
         * start and stop events get called from v8 gc prologue/epilogue
         **/
        void     Start();
        uint64_t Stop();   //< returns elapsed time as given by uv_hrtime()

    private:
        GCStat     stats_;
        uint64_t   startTime_;

        // lock used to prevent reading/writing GCStat data structure at the same time
        // by more than one thread
        uv_mutex_t  lock_;

        /* prevent copying/assigning */
        GCUsage(const GCUsage&);
        GCUsage& operator=(const GCUsage&);
    };

/**
 * General interface for tracking garbage collection statistics/usage
 */
class GCUsageTracker {
public:
    GCUsageTracker() : totalCollections_(0), totalElapsedTime_(0) {};
    ~GCUsageTracker() {};

    //< Call this when a GC is starting
    void StartGC( const v8::GCType type ) {
        GetGCUsage(type).Start();
    }

    //< call when GC is ending
    void StopGC( const v8::GCType type ) {
        uint64_t elapsed = GetGCUsage(type).Stop();
        totalCollections_++;
        totalElapsedTime_ += elapsed;
    }

    inline GCUsage& GetGCUsage( const v8::GCType type ) {
        int collector = v8GCTypeToIndex( type );
        assert( collector >= 0 && collector < kNumGCTypes );

        return usage_[ v8GCTypeToIndex( type )];
    }

    inline unsigned long int totalCollections() const { return totalCollections_; }
    inline uint64_t totalElapsedTime() const { return totalElapsedTime_; }

    static const int kNumGCTypes = 2;  //< must match # of distinct v8::GCTypes in v8.h

    static inline int v8GCTypeToIndex( const v8::GCType type ) {
        return type == v8::kGCTypeScavenge ? 0 : 1;
    }

    static inline v8::GCType indexTov8GCType( const int type ) {
        assert( type < kNumGCTypes );

        switch (type) {
        case 0:  return v8::kGCTypeScavenge;
        case 1:  return v8::kGCTypeMarkSweepCompact;
        default:
            return v8::kGCTypeAll;  //< with assertion above, this should be unreachable
        }
    }

    static const char* indexToString( const int type ) {
        assert( type < kNumGCTypes );

        switch (type) {
        case 0:  return "scavenge";
        case 1:  return "marksweep";
        default:
            return "unknown";  //< with assertion above, this should be unreachable
        }
    }

private:
    GCUsage           usage_[kNumGCTypes];
    long unsigned int totalCollections_;
    uint64_t          totalElapsedTime_;
};



class GCMonitor {
public:
    static void Initialize(v8::Isolate* isolate);

    void Start( v8::Local<v8::Value> arg );
    void Stop();
    virtual ~GCMonitor();

    v8::Isolate* getIsolate() { return isolate_; };
    GCUsageTracker& getGCUsageTracker() { return gcTracker_; }

    static GCMonitor& getInstance();
    static const char* getJsObjectName() { return getInstance().jsObjectName_; }

protected:
    GCMonitor(v8::Isolate* isolate);

private:
    void InitializeProcessObject();
    bool running_;
    const char* jsObjectName_;

    GCUsageTracker gcTracker_;
    v8::Isolate* isolate_;

    uv_async_t check_loop_;

    static GCMonitor* instance_;

    // null private copy/assignment constructors
    GCMonitor(const GCMonitor&);
    const GCMonitor operator=(const GCMonitor&);
};

}

#endif  /* GC_MONITOR_H_ */
