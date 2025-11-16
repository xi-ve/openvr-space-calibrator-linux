#pragma once

#include "Logging.h"
#include <map>
#include <string>
#include <sys/mman.h>
#include <unistd.h>
#include <cstring>
#include <cstdint>

class IHook
{
public:
	const std::string name;

	IHook(const std::string &name) : name(name) { }
	virtual ~IHook() { }

	virtual void Destroy() = 0;

	static bool Exists(const std::string &name);
	static void Register(IHook *hook);
	static void Unregister(IHook *hook);
	static void DestroyAll();

private:
	static std::map<std::string, IHook *> hooks;
};

template<class FuncType> class Hook : public IHook
{
public:
	FuncType originalFunc = nullptr;
	Hook(const std::string &name) : IHook(name) { }

	bool CreateHookInObjectVTable(void *object, int vtableOffset, void *detourFunction)
	{
		void **vtable = *((void ***)object);
		targetFunc = (void**)&vtable[vtableOffset];
		originalFunc = (FuncType)vtable[vtableOffset];

		long pageSize = sysconf(_SC_PAGESIZE);
		void *pageStart = (void*)((uintptr_t)targetFunc & ~(pageSize - 1));
		
		if (mprotect(pageStart, pageSize, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
			LOG("Failed to make vtable writable for %s: %s", name.c_str(), strerror(errno));
			return false;
		}

		*targetFunc = detourFunction;

		if (mprotect(pageStart, pageSize, PROT_READ | PROT_EXEC) != 0) {
			LOG("Failed to restore vtable protection for %s: %s", name.c_str(), strerror(errno));
		}

		LOG("Enabled hook for %s", name.c_str());
		enabled = true;
		return true;
	}

	void Destroy()
	{
		if (enabled && targetFunc)
		{
			long pageSize = sysconf(_SC_PAGESIZE);
			void *pageStart = (void*)((uintptr_t)targetFunc & ~(pageSize - 1));
			
			if (mprotect(pageStart, pageSize, PROT_READ | PROT_WRITE | PROT_EXEC) == 0) {
				*targetFunc = (void*)originalFunc;
				mprotect(pageStart, pageSize, PROT_READ | PROT_EXEC);
			}
			enabled = false;
		}
	}

private:
	bool enabled = false;
	void** targetFunc = nullptr;
};

