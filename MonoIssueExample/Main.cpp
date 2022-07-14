#include <iostream>
#include <mono/jit/jit.h>
#include <mono/metadata/appdomain.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/exception.h>
#include <mono/metadata/mono-debug.h>
#include <mono/metadata/threads.h>

#include <fstream>
#include <string>
#include <filesystem>

static MonoDomain* RootDomain = nullptr;
static MonoDomain* ScriptsDomain = nullptr;

char* ReadBytes(const char* filepath, uint32_t* outSize)
{
	std::ifstream stream(filepath, std::ios::binary | std::ios::ate);

	if (!stream)
	{
		// Failed to open the file
		return nullptr;
	}

	std::streampos end = stream.tellg();
	stream.seekg(0, std::ios::beg);
	uint32_t size = end - stream.tellg();

	if (size == 0)
	{
		// File is empty
		return nullptr;
	}

	char* buffer = new char[size];
	stream.read((char*)buffer, size);
	stream.close();

	*outSize = size;
	return buffer;
}

MonoAssembly* LoadMonoAssembly(const char* assemblyPath)
{
	uint32_t size = 0;
	char* buffer = ReadBytes(assemblyPath, &size);

	MonoImageOpenStatus status;
	MonoImage* image = mono_image_open_from_data_full(buffer, size, 1, &status, 0);

	bool loadDebugSymbols = true;
	std::filesystem::path pdbPath = std::filesystem::path(std::string(assemblyPath) + ".pdb");
	if (!std::filesystem::exists(pdbPath))
	{
		// Otherwise try just .pdb
		pdbPath = assemblyPath;
		pdbPath.replace_extension("pdb");

		if (!std::filesystem::exists(pdbPath))
		{
			loadDebugSymbols = false;
		}
	}

	if (loadDebugSymbols)
	{
		uint32_t size = 0;
		char* buffer = ReadBytes(pdbPath.string().c_str(), &size);
		mono_debug_open_image_from_memory(image, (mono_byte*)buffer, size);
	}

	MonoAssembly* assembly = mono_assembly_load_from_full(image, assemblyPath, &status, 0);
	mono_image_close(image);
	return assembly;
}

void InitMono()
{
	mono_set_assemblies_path("mono/lib");

	std::string portString = std::to_string(2550);
	std::string debuggerAgentArguments = "--debugger-agent=transport=dt_socket,address=127.0.0.1:" + portString + ",server=y,suspend=y,loglevel=3,logfile=logs/MonoDebugger.log";

	// Enable mono soft debugger
	const char* options[2] = {
		debuggerAgentArguments.c_str(),
		"--soft-breakpoints"
	};

	mono_jit_parse_options(2, (char**)options);
	mono_debug_init(MONO_DEBUG_FORMAT_MONO);

	RootDomain = mono_jit_init("HazelJITRuntime");
	mono_debug_domain_create(RootDomain);
	mono_thread_set_main(mono_thread_current());

	ScriptsDomain = mono_domain_create_appdomain((char*)"MyAppDomain", nullptr);
	mono_domain_set(ScriptsDomain, true);
}

MonoClass* GetClassInAssembly(MonoAssembly* assembly, const char* namespaceName, const char* className)
{
	MonoImage* image = mono_assembly_get_image(assembly);
	MonoClass* klass = mono_class_from_name(image, namespaceName, className);
	return klass;
}

void CallMethodViaRuntimeInvoke(MonoClass* instanceClass, MonoObject* instanceObject, float value)
{
	MonoMethod* method = mono_class_get_method_from_name(instanceClass, "CalledViaRuntimeInvoke", 1);
	MonoObject* exception = nullptr;
	void* data = &value;
	mono_runtime_invoke(method, instanceObject, &data, &exception);
}

typedef void(__stdcall* MethodThunk)(MonoObject*, float, MonoException**);
void CallmethodViaUnmanagedThunk(MonoClass* instanceClass, MonoObject* instanceObject, float value)
{
	MonoMethod* method = mono_class_get_method_from_name(instanceClass, "CalledViaUnmanagedThunk", 1);
	MethodThunk thunk = (MethodThunk)mono_method_get_unmanaged_thunk(method);
	MonoException* exception = nullptr;
	thunk(instanceObject, value, &exception);
}

int main()
{
	InitMono();

	MonoAssembly* assembly = LoadMonoAssembly("TestAssembly/TestAssembly/bin/Debug/TestAssembly.dll");
	MonoClass* testingClass = GetClassInAssembly(assembly, "MyAssembly", "AnotherClass");
	MonoObject* classInstance = mono_object_new(ScriptsDomain, testingClass);
	mono_runtime_object_init(classInstance);

	CallMethodViaRuntimeInvoke(testingClass, classInstance, 5.0f);
	CallmethodViaUnmanagedThunk(testingClass, classInstance, 5.0f);

	return 0;
}