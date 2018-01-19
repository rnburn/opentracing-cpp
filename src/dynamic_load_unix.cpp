#include <dlfcn.h>
#include <opentracing/dynamic_load.h>
#include <opentracing/version.h>

namespace opentracing {
BEGIN_OPENTRACING_ABI_NAMESPACE
namespace {
class DynamicLibraryHandleUnix : public DynamicLibraryHandle {
 public:
  explicit DynamicLibraryHandleUnix(void* handle) : handle_{handle} {}

  ~DynamicLibraryHandleUnix() override { dlclose(handle_); }

 private:
  void* handle_;
};
}  // namespace

expected<DynamicTracingLibraryHandle> DynamicallyLoadTracingLibrary(
    const char* shared_library, std::string& error_message) noexcept try {
  dlerror();  // Clear any existing error.

  const auto handle = dlopen(shared_library, RTLD_NOW | RTLD_LOCAL);
  if (handle == nullptr) {
    error_message = dlerror();
    return make_unexpected(dynamic_load_failure_error);
  }

  std::unique_ptr<DynamicLibraryHandle> dynamic_library_handle{
      new DynamicLibraryHandleUnix{handle}};

  const auto make_tracer_factory =
      reinterpret_cast<decltype(OpenTracingMakeTracerFactory)*>(
          dlsym(handle, "OpenTracingMakeTracerFactory"));
  if (make_tracer_factory == nullptr) {
    error_message = dlerror();
    return make_unexpected(dynamic_load_failure_error);
  }

  const void* error_category = nullptr;
  void* tracer_factory = nullptr;
  const auto rcode = make_tracer_factory(OPENTRACING_VERSION, &error_category,
                                         &tracer_factory);
  if (rcode != 0) {
    if (error_category != nullptr) {
      return make_unexpected(std::error_code{
          rcode, *static_cast<const std::error_category*>(error_category)});
    } else {
      error_message = "failed to construct a TracerFactory: unknown error code";
      return make_unexpected(dynamic_load_failure_error);
    }
  }

  if (tracer_factory == nullptr) {
    error_message =
        "failed to construct a TracerFactory: `tracer_factory` is null";
    return make_unexpected(dynamic_load_failure_error);
  }

  return DynamicTracingLibraryHandle{
      std::unique_ptr<const TracerFactory>{
          static_cast<TracerFactory*>(tracer_factory)},
      std::move(dynamic_library_handle)};
} catch (const std::bad_alloc&) {
  return make_unexpected(std::make_error_code(std::errc::not_enough_memory));
}
END_OPENTRACING_ABI_NAMESPACE
}  // namespace opentracing
