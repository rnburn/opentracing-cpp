#include <opentracing/propagation.h>

namespace opentracing {
inline namespace OPENTRACING_VERSION_NAMESPACE {
namespace {
class PropagationErrorCategory : public std::error_category {
 public:
  const char* name() const noexcept override {
    return "OpenTracingPropagationError";
  }

  std::error_condition default_error_condition(int code) const
      noexcept override {
    if (code == unsupported_format_error.value())
      return std::make_error_condition(std::errc::not_supported);
    else if (code == span_context_not_found_error.value())
      return std::make_error_condition(std::errc::invalid_argument);
    else if (code == invalid_span_context_error.value())
      return std::make_error_condition(std::errc::not_supported);
    else if (code == invalid_carrier_error.value())
      return std::make_error_condition(std::errc::invalid_argument);
    else if (code == span_context_corrupted_error.value())
      return std::make_error_condition(std::errc::invalid_argument);
    else
      return std::error_condition(code, *this);
  }

  std::string message(int code) const override {
    if (code == unsupported_format_error.value())
      return "opentracing: Unknown or unsupported Inject/Extract format";
    else if (code == span_context_not_found_error.value())
      return "opentracing: SpanContext not found in Extract carrier";
    else if (code == invalid_span_context_error.value())
      return "opentracing: SpanContext type incompatible with tracer";
    else if (code == invalid_carrier_error.value())
      return "opentracing: Invalid Inject/Extract carrier";
    else if (code == span_context_corrupted_error.value())
      return "opentracing: SpanContext data corrupted in Extract carrier";
    else
      return "opentracing: unknown propagation error";
  }
};
}  // anonymous namespace

const std::error_category& propagation_error_category() {
  static const PropagationErrorCategory error_category;
  return error_category;
}
}  // namespace OPENTRACING_VERSION_NAMESPACE
}  // namespace opentracing