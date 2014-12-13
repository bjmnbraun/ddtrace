#include <signal.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>

#include "DDTrace.h"

namespace DDTrace {

RecordSink recordSinks[MAX_THREADS];
ThreadInitializer threadInitializer;
__thread ThreadId threadid = THREAD_UNCLAIMED;
__thread bool threadInitialized = false;
RecordSource recordSource;
CounterType counterType;
PerfCounters perfCounters[MAX_THREADS];
SLARules slaRules;
uint16_t serverId;
bool initialized = false;

// Currently default to true. Make it configurable in the future.
bool RecordSink::enabled = true;

/**
  * Start a new thread to do the actual work, so the signal handler can return
  * and not block an arbitrary thread, which may cause a deadlock.
  *
  * TODO: verify that this works wiht std_uniqueptr
void terminationHandler(int signo) {
    std::thread(&RecordSink::terminateBackgroundThread,
            &recordKeeper).detach();
}
 */

/**
  * Ensure that the singleton instance of RecordSink is initialized.
  *
  * This function must be called before anything can be used. 
  *
  * Otherwise we will not be able to collect counters and drop them on the
  * floor. 
  *
  * Note that you still have to call the init of RecordSink or RecordSource, if
  * you would like to use either of those classes afterwards. 
  *
  * TODO: Put a NULL check in the interval class constructor after we have
  * imported it.
  */
void init(CounterType type, uint16_t serverId) {
    //recordSink.init(logFile);
    counterType = type;
    DDTrace::serverId = serverId;
    // Configure counters if necessary.
    // Register signal handler to flush buffers
    //signal(SIGTERM, terminationHandler);
    initialized = true;
}

void initThread() {
    threadInitializer.initThread();
    //perfCounters needs to be initialized on each thread
    perfCounters[threadid].init();
    threadInitialized = true;
}

void initThreadSink(const std::string& logName){
    initThread();
    recordSinks[threadid].init(logName);
}

RecordSink::RecordSink()
    : logFile()
{ }

//TODO(bjmnbraun@gmail.com) breaches of style...
std::string storageDirRoot("/dev/shm/");
std::string tempStorageName("temp");
std::string storageName("storage");
std::string makeStorageDirname(const std::string& baseName){
    return storageDirRoot + baseName; 
}
std::string makeStorageInnerDirname(const std::string& baseName){
    return makeStorageDirname(baseName) + "/" + getRecordStateSchema(); 
}
//END breaches of style

RecordStorage* RecordStorageUtils::openStorageFile(const std::string& storageFile){
    int rc;
    int fd = open(storageFile.c_str(), O_RDWR);
    RecordStorage* recordStorage;
    assert(fd >= 0);
    if (!(fd >= 0)) {
        goto err;
    }

    recordStorage = static_cast<RecordStorage*>(mmap(
             NULL, 
             sizeof(RecordStorage), 
             PROT_READ | PROT_WRITE,
             MAP_SHARED,
             fd,
             0));

    assert(recordStorage != MAP_FAILED);
    if (!(recordStorage != MAP_FAILED)) {
        goto err;
    }

    rc = close(fd);
    assert(rc == 0);
    if (!(rc == 0)) {
        goto err;
    }

    return recordStorage;

err:
    throw std::runtime_error("Could not open storage file");
}

std::atomic<ChannelsVersion>* ChannelsVersionUtils::openChannelsVersion(const std::string& baseName){
    std::string schemaDir = makeStorageInnerDirname(baseName);
    std::string channelVersionsFile = schemaDir + "/channelsVersions";
    std::atomic<ChannelsVersion>* channelsVersion;
    int rc = 0;

    //open makes the file get permissions & umask
    //so temporarily override umask
    mode_t oldUmask = umask(0);
    int fd = open(channelVersionsFile.c_str(), O_CREAT | O_RDWR, 0667); //nonexclusive
    umask(oldUmask);
    assert(fd >= 0);
    if (!(fd >= 0)) {
        goto err;
    }
    //ftruncate file to the right size
    //multiple consistent ftruncates do not error
    //the first ftruncate zeros out the atomic
    //hmm. This may not ensure correct initialization of the atomic. Oh well, it'll
    //work on x86 at least, where atomic is essentially a typedef - TODO portability
    rc = ftruncate(fd, sizeof(std::atomic<ChannelsVersion>));
    assert(rc == 0);
    if (!(rc == 0)){
        goto err;
    }

    channelsVersion = 
        static_cast<std::atomic<ChannelsVersion>*>(mmap(
        NULL,
        sizeof(std::atomic<ChannelsVersion>),
        PROT_READ | PROT_WRITE,
        MAP_SHARED,
        fd,
        0));

    assert(channelsVersion != MAP_FAILED);
    if(!(channelsVersion != MAP_FAILED)){
        goto err;
    }

    return channelsVersion;
err:
    throw std::runtime_error("Could not open shared ChannelsVersion");
}

int mkPublicDir(const std::string& dirName){
  int rc;
  
  //mkdir makes the directory get
  //permissions mode & ~umask & 01777.
  //so temporarily override umask
  mode_t oldUmask = umask(0);
  rc = mkdir(dirName.c_str(), S_IRWXU | S_IRWXG | S_IRWXO);
  umask(oldUmask);
  return rc;
}

int makeSHMDirs(const std::string& baseName){
    int rc = 0;
    std::string storageDir = makeStorageDirname(baseName);
    std::string schemaDir = makeStorageInnerDirname(baseName);
    rc = mkPublicDir(storageDir.c_str());
    if (rc == 0){ 
        //O.K.
    } else {
        if (errno == EEXIST){
            //Also O.K.
        } else {
            goto err;
        }
    }

    rc = mkPublicDir(schemaDir.c_str());
    if (rc == 0){ 
        //O.K.
    } else {
        if (errno == EEXIST){
            //Also O.K.
        } else {
            goto err;
        }
    }
    return 0;
err:
    return 1;
}

void RecordSink::init(const std::string& baseName) {
    std::string storageDir = makeStorageDirname(baseName);
    int rc;
    std::atomic<ChannelsVersion>* channelsAvailableVersion;
    const int tempNameLen = 1024;
    char tempName [tempNameLen];
    char finalName [tempNameLen];
    int written;
    int fd;
    std::string schemaDir = makeStorageInnerDirname(baseName);

    rc = makeSHMDirs(baseName);
    if (rc){
        goto err;
    }

    //Make a temporary filename inside schemaDir
    written = snprintf(tempName, 1024, "%s/tmp_XXXXXX", schemaDir.c_str()); 
    assert(written < tempNameLen);
    if (!(written < tempNameLen)){
        goto err;
    }
    fd = mkstemp(tempName);
    assert(fd >= 0);
    if (!(fd >= 0)){
        goto err;
    }
    rc = fchmod(fd, 0666);
    assert(rc == 0);
    if (!(rc == 0)){
        goto err;
    }
    //ftruncate file to the right size
    rc = ftruncate(fd, sizeof(*records));
    assert(rc == 0);
    if (!(rc == 0)){
        goto err;
    }
    records = static_cast<decltype(records)>(mmap(
                NULL, 
                sizeof(*records), 
                PROT_READ | PROT_WRITE,
                MAP_SHARED,
                fd,
                0));
    assert(records != MAP_FAILED);
    if (!(records != MAP_FAILED)){
        goto err;
    }
    rc = close(fd);
    assert(rc == 0);
    if (!(rc == 0)){
        goto err;
    }

    //Placement new constructor
    //see http://stackoverflow.com/questions/25309356/using-new-with-decltype
    new (records) std::remove_pointer<decltype(records)>::type;

    //Replace "tmp" with "rec"
    strcpy(finalName, tempName);
    memcpy(finalName + strlen(finalName) - 10, "rec", 3);
    rc = rename(tempName, finalName);
    assert(rc == 0);
    if (!(rc == 0)) {
        goto err;
    }
    
    channelsAvailableVersion = ChannelsVersionUtils::openChannelsVersion(baseName);
    //This atomic add will lock the cache line, ensuring that the value changes
    //This isn't actually necessary - if two threads simultaneously add, we're
    //O.K. with only one increment occuring because it will still correctly
    //cause any sources to look for new channels
    //Whatever, this is not worth optimizing.
    channelsAvailableVersion->fetch_add(1);
    //Finally, close the mmap
    rc = munmap(channelsAvailableVersion, sizeof(channelsAvailableVersion));
    assert(rc == 0);
    if (!(rc == 0)) {
        goto err;
    }
    return;
err:
    fprintf(stderr, "Could not create RecordSink inside %s\n", storageDir.c_str());
    throw std::runtime_error("Could not create RecordSink file");
}

RecordSink::~RecordSink() {
//    terminateBackgroundThread();
}

void RecordSource::init(const std::string& _baseName) {
    baseName = _baseName;
    int rc = makeSHMDirs(baseName);
    assert(rc == 0);
    if (!(rc == 0)){
        goto err;
    }
    channelsAvailableVersion = ChannelsVersionUtils::openChannelsVersion(baseName);
    updateChannels();
    return;
err:
    throw std::runtime_error("Could not initialize RecordSource");
}

/**
  * Close the mapping to a record sink and delete the backing file
  * Should only be called after the channel is dead
  */
void closeAndRemoveRecordSink(const std::string& fileName, 
                              RecordStorage* records){
    assert(records);
    int rc;
    rc = munmap(records, sizeof(RecordStorage));
    assert(rc == 0);
    if (!(rc == 0)){
        goto err;
    }
    rc = remove(fileName.c_str());
    assert(rc == 0);
    if (!(rc == 0)){
        goto err;
    }
    return;
    err:
    fprintf(stderr, "Could not remove dead channel %s\n", fileName.c_str());
    throw std::runtime_error("Could not remove dead channel\n");
}

#define DEBUG_CHANNEL_DETECTION 1
void RecordSource::cleanupDeadChannels(){
    //Iterate through recordStorageSet and remove any dead entries      
    for(auto itr = recordStorageSet.begin();
        itr != recordStorageSet.end();
        ){
        const std::string& fileName = itr->first;
        int fileRefCount = Util::getNumFileReferences(fileName);
        //Hmph... fileRefCount is flaking out and not always returning
        //our reference. This only seems to happen rarely.
        //assert(fileRefCount > 0); //We are referencing this file
        assert(fileRefCount < 3); //There is at most one other reference
        bool should_delete = false;
        if (fileRefCount <= 1){  //At most us referencing the file
#if DEBUG_CHANNEL_DETECTION == 1
            fprintf(stderr,"Detected dead channel: %s\n", fileName.c_str());
#endif
            should_delete = true;
        }
        if (should_delete){
            closeAndRemoveRecordSink(fileName, itr->second);
            itr = recordStorageSet.erase(itr);
        } else {
            ++itr;
        }
    }
    recordsIterator = recordStorageSet.begin();
}

void RecordSource::updateChannels(){
    //scan for any new record sinks
    std::string schemaDir = makeStorageInnerDirname(baseName);
    DIR* channels = opendir(schemaDir.c_str());
    struct dirent* entry;
    if (!channels){
        //This can occur if the directory hasn't been created yet
        goto out; 
    }
    while((entry = readdir(channels)) != NULL){
        //Only pay attention to files that have a "rec_" prefix
        if (strstr(entry->d_name, "rec_") ==
                entry->d_name){
            //Valid channel
            std::string fileName = schemaDir+"/"+entry->d_name;
            //Do we already have this channel open?
            if (recordStorageSet.find(fileName) == recordStorageSet.end()){
#if DEBUG_CHANNEL_DETECTION == 1
                fprintf(stderr,"Found new channel: %s\n", fileName.c_str());
#endif
                RecordStorage* records = RecordStorageUtils::openStorageFile(
                    fileName);
                recordStorageSet[fileName] = records;
            }
        }
    }
out:
    //timeLastUpdateRecords = Cycles::rdtsc();    
    channelsVersion = channelsAvailableVersion->load(std::memory_order_relaxed);
    recordsIterator = recordStorageSet.begin();
}

} //namespace DDTrace
