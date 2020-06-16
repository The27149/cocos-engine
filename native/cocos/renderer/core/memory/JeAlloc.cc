#include "CoreStd.h"
#include "JeAlloc.h"
#include "MemTracker.h"

#if (CC_MEMORY_ALLOCATOR == CC_MEMORY_ALLOCATOR_JEMALLOC)

#include "jemalloc/jemalloc.h"
extern const char* je_malloc_conf = "narenas:4";

namespace cc {

#ifdef CC_MEMORY_TRACKER
#define MEM_CHECKTAG_SIZE  4
#define MEM_CHECKTAG       0x20170719

CC_INLINE static void CheckOverflowAlloc(void* ptr) {
	size_t size = je_malloc_usable_size(ptr);
	char* p = (char*)ptr + size - MEM_CHECKTAG_SIZE;
	uint tag = MEM_CHECKTAG;
	memcpy(p, &tag, MEM_CHECKTAG_SIZE);
}

CC_INLINE static void CheckOverflowFree(void* ptr) {
	size_t size = je_malloc_usable_size(ptr);
	char* p = (char*)ptr + size - MEM_CHECKTAG_SIZE;
	uint tag;
	memcpy(&tag, p, MEM_CHECKTAG_SIZE);
	CCASSERT(tag == MEM_CHECKTAG);
}

#else
#define MEM_CHECKTAG_SIZE  0
#endif

CC_DECL_MALLOC void* JeAllocImpl::AllocBytes(size_t count, const char* file, int line, const char* func) {
	void* ptr = je_malloc(count + MEM_CHECKTAG_SIZE);
#ifdef CC_MEMORY_TRACKER
	CheckOverflowAlloc(ptr);
	MemTracker::Instance()->RecordAlloc(ptr, count, file, line, func);
#else
	// avoid unused params warning
	(void)file;
	(void)line;
	(void)func;
#endif
	return ptr;
}

CC_DECL_MALLOC void* JeAllocImpl::ReallocBytes(void* ptr, size_t count, const char* file, int line, const char* func) {
#ifdef CC_MEMORY_TRACKER
	if (ptr)
	{
		if(count == 0) {
			CheckOverflowFree(ptr);
			MemTracker::Instance()->RecordFree(ptr);
			je_free(ptr);
			return nullptr;
		} else {
			size_t oldsz = je_malloc_usable_size(ptr) - MEM_CHECKTAG_SIZE;
			if (oldsz > count) oldsz = count;

			void* nptr = je_malloc(count + MEM_CHECKTAG_SIZE);
			memcpy(nptr, ptr, oldsz);
			CheckOverflowAlloc(nptr);
			MemTracker::Instance()->RecordAlloc(nptr, count, file, line, func);

			CheckOverflowFree(ptr);
			MemTracker::Instance()->RecordFree(ptr);
			je_free(ptr);
			return nptr;
		}
	} else {
		if (count) {
			ptr = je_malloc(count + MEM_CHECKTAG_SIZE);
			CheckOverflowAlloc(ptr);
			MemTracker::Instance()->RecordAlloc(ptr, count, file, line, func);
			return ptr;
		} else {
			return nullptr;
		}
	}
#else
	// avoid unused params warning
	(void)file;
	(void)line;
	(void)func;
	return je_realloc(ptr, count);
#endif
}

CC_DECL_MALLOC void* JeAllocImpl::AllocBytesAligned(size_t align, size_t count, const char* file, int line, const char* func) {
	// default to platform SIMD alignment if none specified
	void* ptr = je_aligned_alloc(align, count + MEM_CHECKTAG_SIZE);
#ifdef COCOS_MEMORY_TRACKER
	CheckOverflowAlloc(ptr);
	MemTracker::Instance()->RecordAlloc(ptr, count, file, line, func);
#else
	// avoid unused params warning
	(void)file;
	(void)line;
	(void)func;
#endif
	return ptr;
}

void JeAllocImpl::DeallocBytes(void* ptr) {
	// deal with null
	if (!ptr)
		return;
#ifdef CC_MEMORY_TRACKER
	CheckOverflowFree(ptr);
	MemTracker::Instance()->RecordFree(ptr);
#endif
	je_free(ptr);
}

struct JeDumpData {
	char* buf;
	int size;
};

static void JeStatsPrint(void* param, const char* msg) {
	JeDumpData* jd = (JeDumpData*)param;
	if (jd->size == 0) return;

	int len = (int)strlen(msg);
	if (len > jd->size) len = jd->size;
	memcpy(jd->buf, msg, len);
	jd->buf[len] = 0;
	jd->buf += len;
	jd->size -= len;
}

void JeAllocImpl::DumpStats(char* buf, int bufsize) {
	JeDumpData jd;
	jd.buf = buf;
	jd.size = bufsize - 1;
	je_malloc_stats_print(JeStatsPrint, &jd, "ma");
}

void JeAllocImpl::TrimAlloc() {
	int narenas = 0;
	size_t sz = sizeof(narenas);
	je_mallctl("arenas.narenas", (void *)&narenas, &sz, nullptr, 0);
	char buf[24];
	sprintf(buf, "arena.%d.purge", narenas);
	printf("jemalloc trim: %s\n", buf);
	je_mallctl(buf, NULL, NULL, NULL, 0);
}

}

#endif // end - #ifdef CC_MEMORY_ALLOCATOR_JEMALLOC
